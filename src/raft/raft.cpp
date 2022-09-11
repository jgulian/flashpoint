#include <utility>

#include "raft/raft.hpp"

namespace flashpoint::raft {

ExtendedRaftPeer::ExtendedRaftPeer(std::shared_ptr<protos::raft::Raft::StubInterface> connection)
	: connection(std::move(connection)) {}
bool ExtendedRaftPeer::Active() const { return active_in_base_config || !active_in_configs.empty(); }

Raft::Raft(std::shared_ptr<RaftSettings> config, bool use_persisted_state)
	: settings_(std::move(config)) {
  RegisterNewConfig(0, settings_->starting_config, true);
  raft_state_->set_log_offset(1);
  raft_state_->set_log_size(1);
  raft_state_->set_current_term(0);
  raft_state_->clear_voted_for();

  for (auto &kPeer : settings_->starting_config.peers()) {
	auto new_peer = raft_state_->mutable_peers()->Add();
	new_peer->mutable_peer()->CopyFrom(kPeer);
	new_peer->set_next_index(1);
	new_peer->set_match_index(0);
	new_peer->set_snapshot_id(0);
	new_peer->set_chunk_offset(0);
  }
}

bool Raft::Run() {
  bool running = false;
  bool successful = running_->compare_exchange_strong(running, true);
  if (!successful && running)
	throw std::runtime_error("raft is already running");
  else if (!successful)
	return false;

  worker_function_ = std::make_unique<std::thread>(&Raft::Worker, this);
  return true;
}
bool Raft::Kill() {
  bool running = false;
  bool successful = running_->compare_exchange_strong(running, true);
  if (!successful && running)
	throw std::runtime_error("raft is not running");
  else if (!successful)
	return false;

  worker_function_->join();
  return true;
}

bool Raft::Snapshot(LogIndex included_index) {
  std::lock_guard<std::mutex> lock_guard(*lock_);

  if (!settings_->persistence_settings.has_value())
	throw std::runtime_error("can't Update Snapshot when snapshots are not enabled");

  if (role_ != LEADER)
	return false;

  auto snapshot = raft_state_->mutable_snapshot();
  snapshot->set_snapshot_id(raft_state_->snapshot().snapshot_id() + 1);
  snapshot->set_last_included_index(included_index);
  snapshot->set_last_included_term(AtLogIndex(included_index).term());

  auto file = std::filesystem::path(settings_->persistence_settings->snapshot_file);
  snapshot->set_file_size(std::filesystem::file_size(file));
  snapshot->set_chunk_count((snapshot->file_size() / 1000000 / 256) + 1);

  raft_state_->mutable_entries()->DeleteSubrange(0, static_cast<int>(included_index - raft_state_->log_offset()) + 1);
  raft_state_->set_log_offset(included_index + 1);

  return true;
}

void Raft::Worker() {
  while (running_) {
	auto current_time = std::chrono::system_clock::now();

	{
	  std::lock_guard<std::mutex> lock_guard(*lock_);

	  if (role_ == FOLLOWER) {
		if (std::chrono::system_clock::now() - last_heartbeat_ > kElectionTimeout) {
		  raft_state_->set_current_term(raft_state_->current_term() + 1);
		  role_ = CANDIDATE;
		  sent_vote_requests_ = false;
		}
	  } else if (role_ == CANDIDATE) {
		last_heartbeat_ = std::chrono::system_clock::now();
		UpdateLeaderElection();
	  } else if (role_ == LEADER) {
		last_heartbeat_ = std::chrono::system_clock::now();
		UpdateIndices();
		UpdateFollowers();
	  }

	  CommitEntries();
	  Persist();
	}

	auto sleep_for = settings_->random->RandomDurationBetween(kMinSleepTime, kMaxSleepTime);
	if (std::chrono::system_clock::now() < current_time + sleep_for)
	  std::this_thread::sleep_until(current_time + sleep_for);
	else
	  util::GetLogger()->Msg(util::LogLevel::WARN, "raft Worker sleep cycle missed");
  }
}
void Raft::UpdateLeaderElection() {
  if (!sent_vote_requests_) {
	votes_received_.clear();
	if (extended_peers_->at(settings_->me.id())->Active())
	  votes_received_.emplace_back(settings_->me.id());
	raft_state_->set_voted_for(settings_->me.id());

	protos::raft::RequestVoteRequest request = {};
	request.set_term(raft_state_->current_term());
	request.set_candidate_id(settings_->me.id());
	request.set_last_log_index(raft_state_->log_size() - 1);
	request.set_last_log_term(0 < raft_state_->log_size() - raft_state_->log_offset()
							  ? AtLogIndex(request.last_log_index()).term()
							  : raft_state_->snapshot().last_included_term());

	for (auto &[peer_id, peer] : *extended_peers_) {
	  if (!peer->Active() || peer_id == settings_->me.id())
		continue;
	  auto call = std::make_shared<
		  ExtendedRaftPeer::Call<protos::raft::RequestVoteRequest, protos::raft::RequestVoteResponse >>();
	  peer->last_call = call;
	  call->request = request;
	  peer->connection->async()->RequestVote(&call->client_context, &call->request, &call->response,
											 [call](const grpc::Status &status) {
											   call->status = status;
											   call->complete = true;
											 });
	}
	sent_vote_requests_ = true;
  } else {
	auto remaining_voter_count = 0;

	for (auto &[peer_id, peer] : *extended_peers_) {
	  if (!peer->Active() || peer_id == settings_->me.id())
		continue;
	  if (!std::holds_alternative<ExtendedRaftPeer::RequestVoteCall>(peer->last_call))
		continue;

	  auto &call = std::get<ExtendedRaftPeer::RequestVoteCall>(peer->last_call);
	  if (!call->complete) {
		remaining_voter_count++;
		continue;
	  }
	  auto &response = call->response;

	  if (response.vote_granted()) {
		votes_received_.emplace_back(peer_id);
	  } else if (raft_state_->current_term() < response.term()) {
		raft_state_->set_current_term(response.term());
		leader_ = response.leader_id();
		role_ = FOLLOWER;
		for (auto &[_, p] : *extended_peers_)
		  peer->last_call = std::monostate();
		return;
	  }
	}

	if (CheckAllConfigsAgreement(votes_received_)) {
	  role_ = LEADER;
	  leader_ = settings_->me.id();
	  for (auto &peer : *raft_state_->mutable_peers()) {
		peer.set_match_index(peer.peer().id() == settings_->me.id() ? (raft_state_->log_size() - 1) : 0);
		peer.set_next_index(raft_state_->log_size());
	  }
	} else if (remaining_voter_count == 0) {
	  role_ = FOLLOWER;
	}
  }
}
void Raft::UpdateIndices() {
  while (commit_index_ < raft_state_->log_size() && CheckAllConfigsAgreement(AgreersForIndex(commit_index_ + 1)))
	commit_index_++;
}

void Raft::UpdateFollowers() {
  for (auto &peer : *raft_state_->mutable_peers()) {
	auto &extended_peer = extended_peers_->at(peer.peer().id());

	if (settings_->me.id() == peer.peer().id())
	  continue;
	if (std::holds_alternative<ExtendedRaftPeer::InstallSnapshotCall>(extended_peer->last_call)) {
	  const auto kCall = std::get<ExtendedRaftPeer::InstallSnapshotCall>(extended_peer->last_call);
	  if (!kCall->complete)
		continue;
	  if (!kCall->status.ok()) {
		UpdateFollower(peer, extended_peer);
		continue;
	  }

	  if (raft_state_->current_term() < kCall->response.term()) {
		raft_state_->set_current_term(kCall->response.term());
		leader_ = kCall->response.leader_id();
		role_ = FOLLOWER;
		return;
	  }

	  peer.set_snapshot_id(kCall->request.snapshot_id());
	  peer.set_chunk_offset(kCall->request.chunk_offset() + 1);
	  UpdateFollower(peer, extended_peer);
	} else if (std::holds_alternative<ExtendedRaftPeer::AppendEntriesCall>(extended_peer->last_call)) {
	  const auto kCall = std::get<ExtendedRaftPeer::AppendEntriesCall>(extended_peer->last_call);
	  if (!kCall->complete)
		continue;
	  if (!kCall->status.ok()) {
		UpdateFollower(peer, extended_peer);
		continue;
	  }

	  if (raft_state_->current_term() < kCall->response.term()) {
		raft_state_->set_current_term(kCall->response.term());
		leader_ = kCall->response.leader_id();
		role_ = FOLLOWER;
		return;
	  }

	  if (kCall->response.success()) {
		if (!kCall->request.entries().empty()) {
		  auto updated_to = kCall->request.entries(kCall->request.entries_size() - 1).index();
		  peer.set_match_index(updated_to);
		  peer.set_next_index(updated_to + 1);
		}
	  } else {
		auto next_index = kCall->response.conflict_index();
		if (next_index <= raft_state_->snapshot().last_included_index() && kCall->response.conflict_term() == -1
			|| kCall->response.conflict_term() < raft_state_->snapshot().last_included_term()
				&& kCall->response.conflict_term() != -1) {
		  // A Snapshot is required
		} else {
		  if (kCall->response.conflict_term() != -1)
			for (auto entry = raft_state_->entries().rbegin(); entry != raft_state_->entries().rend(); ++entry)
			  if (entry->term() == kCall->response.conflict_term()) {
				next_index = entry->index();
				break;
			  }

		  peer.set_next_index(next_index);
		}
	  }

	  UpdateFollower(peer, extended_peer);
	} else {
	  UpdateFollower(peer, extended_peer);
	}
  }
}
void Raft::UpdateFollower(const protos::raft::RaftState_PeerState &peer,
						  const std::unique_ptr<ExtendedRaftPeer> &extended_peer) {
  grpc::ClientContext client_context = {};
  grpc::Status status;

  if (peer.snapshot_id() != raft_state_->snapshot().snapshot_id()
	  || peer.chunk_offset() != raft_state_->snapshot().chunk_count()) {
	auto call = std::make_shared<
		ExtendedRaftPeer::Call<protos::raft::InstallSnapshotRequest, protos::raft::InstallSnapshotResponse >>();
	extended_peer->last_call = call;

	call->request.set_term(raft_state_->current_term());
	call->request.set_leader_id(settings_->me.id());

	call->request.set_last_included_index(raft_state_->snapshot().last_included_index());
	call->request.set_last_included_term(raft_state_->snapshot().last_included_term());
	call->request.set_snapshot_id(raft_state_->snapshot().snapshot_id());
	if (peer.snapshot_id() != raft_state_->snapshot().snapshot_id())
	  call->request.set_chunk_offset(0);
	else
	  call->request.set_chunk_offset(peer.chunk_offset() + 1);
	FillWithChunk(call->request);
	call->request.set_last_chunk(call->request.chunk_offset() + 1 == raft_state_->snapshot().chunk_count());

	extended_peer->connection->async()->InstallSnapshot(&call->client_context, &call->request, &call->response,
														[call](const grpc::Status &status) {
														  call->status = status;
														  call->complete = true;
														});
  } else {
	auto call = std::make_shared<
		ExtendedRaftPeer::Call<protos::raft::AppendEntriesRequest, protos::raft::AppendEntriesResponse >>();
	extended_peer->last_call = call;
	call->request.set_term(raft_state_->current_term());
	call->request.set_leader_id(settings_->me.id());

	call->request.set_prev_log_index(peer.next_index() - 1);
	call->request.set_prev_log_term(peer.next_index() == raft_state_->log_offset()
									? raft_state_->snapshot().last_included_term()
									: AtLogIndex(call->request.prev_log_index()).term());

	for (auto i = peer.next_index(); i < raft_state_->log_size(); i++)
	  call->request.mutable_entries()->Add()->CopyFrom(AtLogIndex(i));
	call->request.set_leader_commit_index(commit_index_);

	extended_peer->connection->async()->AppendEntries(&call->client_context, &call->request, &call->response,
													  [call](const grpc::Status &status) {
														call->status = status;
														call->complete = true;
													  });
  }
}
const protos::raft::LogEntry &Raft::AtLogIndex(LogIndex index) const {
  if (index < raft_state_->log_offset())
	throw RaftException(RaftExceptionType::INDEX_EARLIER_THAN_SNAPSHOT);
  if (raft_state_->log_size() <= index)
	throw RaftException(RaftExceptionType::INDEX_OUT_OF_LOG_BOUNDS);
  return raft_state_->entries(static_cast<int>(index - raft_state_->log_offset()));
}
void Raft::CommitEntries() {
  while (last_applied_ < commit_index_) {
	last_applied_++;
	auto &kEntry = AtLogIndex(last_applied_);

	switch (kEntry.log_data().type()) {
	  case protos::raft::COMMAND: settings_->apply_command(kEntry);
		break;
	  case protos::raft::CONFIG: {
		if (proposed_configs_.empty())
		  throw RaftException(RaftExceptionType::CONFIG_NOT_IN_PROPOSED_CONFIG);
		if (kEntry.index() != proposed_configs_.front())
		  throw RaftException(RaftExceptionType::CONFIG_NOT_IN_PROPOSED_CONFIG);

		raft_state_->mutable_base_config()->ParseFromString(kEntry.log_data().data());
		proposed_configs_.pop_front();
		settings_->apply_config_update(kEntry);
	  }
		break;
	  default: throw RaftException(RaftExceptionType::ATTEMPTED_COMMIT_OF_UNKNOWN_ENTRY);
	}
  }
}
bool Raft::CheckAllConfigsAgreement(const std::list<PeerId> &agreers) const {
  if (!HasAgreement(raft_state_->base_config(), agreers))
	return false;

  for (const auto &kConfigIndex : proposed_configs_) {
	protos::raft::Config config = {};
	config.ParseFromString(AtLogIndex(kConfigIndex).log_data().data());
	if (!HasAgreement(config, agreers))
	  return false;
  }

  return true;
}
bool Raft::HasAgreement(const protos::raft::Config &config, const std::list<PeerId> &agreers) {
  auto agree_count = 0;
  for (const auto &kPeer : agreers) {
	for (const auto &kConfigPeer : config.peers())
	  if (kConfigPeer.id() == kPeer)
		agree_count++;
  }

  return agree_count > (config.peers_size() / 2);
}
std::list<PeerId> Raft::AgreersForIndex(LogIndex index) const {
  std::list<PeerId> result = {};
  for (const auto &kPeer : raft_state_->peers())
	if (index <= kPeer.match_index())
	  result.emplace_back(kPeer.peer().id());
  return result;
}
void Raft::FillWithChunk(protos::raft::InstallSnapshotRequest &request) const {
  if (raft_state_->snapshot().snapshot_id() != request.snapshot_id())
	throw std::runtime_error("attempted to fill request with old Snapshot");

  std::ifstream snapshot_file = {};
  snapshot_file.open(settings_->persistence_settings->snapshot_file);
  snapshot_file.seekg(static_cast<long long>(request.chunk_offset() * kSnapshotChunkSize));

  auto amount_to_read = kSnapshotChunkSize;
  if (request.chunk_offset() == raft_state_->snapshot().chunk_count() - 1)
	amount_to_read = raft_state_->snapshot().file_size() % kSnapshotChunkSize;

  auto chunk_data = std::make_unique<std::string>();
  chunk_data->reserve(amount_to_read);
  snapshot_file.read(chunk_data->data(), static_cast<long long>(amount_to_read));
  request.set_allocated_chunk(chunk_data.release());
}
void Raft::RegisterNewConfig(LogIndex log_index, const protos::raft::Config &config, bool base_config) {
  for (auto &kConfigPeer : config.peers()) {
	if (extended_peers_->contains(kConfigPeer.id())) {
	  auto &peer = extended_peers_->at(kConfigPeer.id());
	  if (!base_config && kConfigPeer.voting())
		peer->active_in_configs.emplace_back(log_index);
	  if (base_config && kConfigPeer.voting())
		peer->active_in_base_config = true;
	} else {
	  auto channel = grpc::CreateChannel(kConfigPeer.data().address(), grpc::InsecureChannelCredentials());
	  auto stub = protos::raft::Raft::NewStub(channel);
	  auto peer = std::make_unique<ExtendedRaftPeer>(std::move(stub));
	  if (!base_config && kConfigPeer.voting())
		peer->active_in_configs.emplace_back(log_index);
	  if (base_config && kConfigPeer.voting())
		peer->active_in_base_config = true;
	  extended_peers_->emplace(kConfigPeer.id(), std::move(peer));
	}
  }

  if (base_config)
	raft_state_->mutable_base_config()->CopyFrom(config);
}

grpc::Status Raft::Start(::grpc::ServerContext *context, const ::protos::raft::StartRequest *request,
						 ::protos::raft::StartResponse *response) {
  std::lock_guard<std::mutex> lock_guard(*lock_);
  if (role_ != LEADER) {
	response->set_leader_id(leader_);
	return grpc::Status::OK;
  }

  auto entry = raft_state_->mutable_entries()->Add();
  LogIndex log_index = raft_state_->log_size();
  raft_state_->set_log_size(log_index + 1);
  entry->set_index(log_index);
  entry->set_term(raft_state_->current_term());
  entry->mutable_log_data()->CopyFrom(request->log_data());
  entry->set_data_valid(false);

  if (request->log_data().type() == protos::raft::LogDataType::CONFIG) {
	protos::raft::Config config = {};
	config.ParseFromString(entry->log_data().data());
	RegisterNewConfig(log_index, config, false);
  }

  for (auto &peer : *raft_state_->mutable_peers())
	if (peer.peer().id() == settings_->me.id())
	  peer.set_match_index(peer.match_index() + 1);

  Persist();
  response->set_log_index(log_index);
  return grpc::Status::OK;
}
grpc::Status Raft::AppendEntries(::grpc::ServerContext *context, const ::protos::raft::AppendEntriesRequest *request,
								 ::protos::raft::AppendEntriesResponse *response) {
  std::lock_guard<std::mutex> lock_guard(*lock_);

  response->set_term(raft_state_->current_term());
  if (request->term() < raft_state_->current_term()) {
	response->set_leader_id(leader_);
	response->set_success(true);
	return grpc::Status::OK;
  } else if (raft_state_->current_term() < request->term()) {
	raft_state_->set_current_term(request->term());
	leader_ = request->leader_id();
	role_ = FOLLOWER;
  } else if (leader_ != request->leader_id()) {
	leader_ = request->leader_id();
  }

  auto last_log_index = raft_state_->log_size() - 1;

  if (last_log_index < request->prev_log_index()) {
	response->set_conflict_index(last_log_index + 1);
	response->set_conflict_term(-1);
	response->set_success(false);
	return grpc::Status::OK;
  } else {
	auto conflict_term = raft_state_->log_size() == raft_state_->log_offset()
						 ? raft_state_->snapshot().last_included_term()
						 : AtLogIndex(raft_state_->log_size() - 1).term();

	if (conflict_term != request->prev_log_term()) {
	  response->set_conflict_term(conflict_term);
	  auto i = request->prev_log_index();
	  while (raft_state_->log_offset() < i && AtLogIndex(i).term() != request->prev_log_term())
		i--;
	  response->set_conflict_index(i);
	  response->set_success(false);
	  return grpc::Status::OK;
	}
  }

  if (request->prev_log_index() < raft_state_->log_size() - 1) {
	auto delete_from = static_cast<int>(request->prev_log_index() - raft_state_->log_offset() + 1);
	auto delete_to = static_cast<int>(raft_state_->log_size() - request->prev_log_index());
	raft_state_->mutable_entries()->DeleteSubrange(delete_from, delete_to);// may be faulty
  }

  leader_ = request->leader_id();
  commit_index_ = std::min(request->leader_commit_index(), last_log_index);
  last_heartbeat_ = std::chrono::system_clock::now();
  for (const auto &kEntry : request->entries()) {
	raft_state_->mutable_entries()->Add()->CopyFrom(kEntry);
	raft_state_->set_log_size(raft_state_->log_size() + 1);
  }

  Persist();
  response->set_success(true);
  return grpc::Status::OK;
}
grpc::Status Raft::RequestVote(::grpc::ServerContext *context, const ::protos::raft::RequestVoteRequest *request,
							   ::protos::raft::RequestVoteResponse *response) {
  std::lock_guard<std::mutex> lock_guard(*lock_);

  response->set_term(raft_state_->current_term());

  auto last_log_index = raft_state_->log_size() - 1;
  auto last_log_term = 0 < raft_state_->log_size() - raft_state_->log_size()
					   ? AtLogIndex(last_log_index).term()
					   : raft_state_->snapshot().last_included_term();

  if (raft_state_->current_term() < request->term()) {
	raft_state_->set_current_term(request->term());
	role_ = FOLLOWER;
	raft_state_->clear_voted_for();
  }

  if (request->term() < raft_state_->current_term()
	  || (!raft_state_->voted_for().empty() && raft_state_->voted_for() != request->candidate_id())
	  || (request->last_log_term() < last_log_term)
	  || (request->last_log_term() == last_log_term && request->last_log_index() < last_log_index)) {
	response->set_vote_granted(false);
	return grpc::Status::OK;
  }

  last_heartbeat_ = std::chrono::system_clock::now();
  response->set_vote_granted(true);
  raft_state_->set_voted_for(request->candidate_id());

  Persist();
  return grpc::Status::OK;
}
grpc::Status Raft::InstallSnapshot(::grpc::ServerContext *context,
								   const ::protos::raft::InstallSnapshotRequest *request,
								   ::protos::raft::InstallSnapshotResponse *response) {
  std::lock_guard<std::mutex> lock_guard(*lock_);

  response->set_term(raft_state_->current_term());

  if (request->term() < raft_state_->current_term()) {
	response->set_leader_id(leader_);
	return grpc::Status::OK;
  } else if (raft_state_->current_term() < request->term()) {
	raft_state_->set_current_term(request->term());
	leader_ = request->leader_id();
	role_ = FOLLOWER;
  } else if (leader_ != request->leader_id()) {
	leader_ = request->leader_id();
  }

  auto temp_file_name = "~" + settings_->persistence_settings->snapshot_file;
  if (request->chunk_offset() == 0) {
	if (std::filesystem::exists(temp_file_name))
	  std::remove(temp_file_name.c_str());
  }

  std::ofstream temp_file(temp_file_name);
  temp_file.seekp(static_cast<long long>(request->chunk_offset() * kSnapshotChunkSize));
  temp_file << request->chunk();

  if (request->last_chunk()) {
	std::ifstream temp_file_read(temp_file_name);
	std::ofstream real_file(settings_->persistence_settings->snapshot_file);
	real_file << temp_file_read.rdbuf();
	std::remove(temp_file_name.c_str());

	auto snapshot = raft_state_->mutable_snapshot();
	snapshot->set_snapshot_id(request->snapshot_id());
	snapshot->set_last_included_index(request->last_included_index());
	snapshot->set_last_included_term(request->last_included_term());
	snapshot->set_file_size(request->chunk_offset() * kSnapshotChunkSize + request->chunk().size());
	snapshot->set_chunk_count(request->chunk_offset() + 1);

	raft_state_->mutable_entries()
		->DeleteSubrange(0, static_cast<int>(request->last_included_index() - raft_state_->log_offset()) + 1);
	raft_state_->set_log_offset(request->last_included_index() + 1);
  }

  Persist();
  return grpc::Status::OK;
}

void Raft::Persist() {
  if (!settings_->persistence_settings.has_value())
	return;

  std::ofstream persistent_file = {};
  persistent_file.open(settings_->persistence_settings.value().persistent_file);
  raft_state_->SerializeToOstream(&persistent_file);
  auto written_count = persistent_file.tellp();
  persistent_file.close();

  settings_->persistence_settings->recent_persists.Push(written_count);
}

}// namespace flashpoint::raft