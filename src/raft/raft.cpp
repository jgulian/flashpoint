#include <utility>

#include "raft/raft.hpp"

namespace flashpoint::raft {

bool hasAgreement(const protos::raft::Config &config, const std::list<PeerId> &agreers) {
  auto agree_count = 0;
  for (const auto &peer : agreers) {
    for (const auto &config_peer : config.peers())
      if (config_peer.id() == peer) agree_count++;
  }

  return agree_count > (config.peers_size() / 2);
}

ExtendedRaftPeer::ExtendedRaftPeer(std::shared_ptr<protos::raft::Raft::StubInterface> connection)
    : connection(std::move(connection)) {}
bool ExtendedRaftPeer::active() const { return active_in_base_config || !active_in_configs.empty(); }

Raft::Raft(std::unique_ptr<RaftConfig> config, const protos::raft::RaftState &save_state)
    : raft_config_(std::move(config)) {
  registerNewConfig(0, raft_config_->starting_config, true);
  raft_state_->set_log_offset(1);
  raft_state_->set_log_size(1);
  raft_state_->set_current_term(0);
  raft_state_->clear_voted_for();

  for (auto &peer : raft_config_->starting_config.peers()) {
    auto new_peer = raft_state_->mutable_peers()->Add();
    new_peer->mutable_peer()->CopyFrom(peer);
    new_peer->set_next_index(1);
    new_peer->set_match_index(0);
    new_peer->set_snapshot_id(0);
    new_peer->set_chunk_offset(0);
  }
}

bool Raft::run() {
  bool running = false;
  bool successful = running_->compare_exchange_strong(running, true);
  if (!successful && running) throw std::runtime_error("raft is already running");
  else if (!successful)
    return false;

  worker_function_ = std::make_unique<std::thread>(&Raft::worker, this);
  return true;
}
bool Raft::kill() {
  bool running = false;
  bool successful = running_->compare_exchange_strong(running, true);
  if (!successful && running) throw std::runtime_error("raft is not running");
  else if (!successful)
    return false;

  worker_function_->join();
  return true;
}

bool Raft::snapshot(LogIndex included_index, std::string snapshot_file) {
  std::lock_guard<std::mutex> lock_guard(*lock_);

  if (role_ != LEADER) return false;

  auto snapshot = raft_state_->mutable_snapshot();
  snapshot->set_snapshot_id(raft_state_->snapshot().snapshot_id() + 1);
  snapshot->set_last_included_index(included_index);
  snapshot->set_last_included_term(atLogIndex(included_index).term());

  snapshot->set_file_size(std::filesystem::file_size(std::filesystem::path(snapshot_file)));
  snapshot->set_file(std::move(snapshot_file));
  snapshot->set_chunk_count((snapshot->file_size() / 1000000 / 256) + 1);

  raft_state_->mutable_entries()->DeleteSubrange(0, static_cast<int>(included_index - raft_state_->log_offset()));

  return true;
}

void Raft::worker() {
  while (running_) {
    auto current_time = std::chrono::system_clock::now();

    {
      std::lock_guard<std::mutex> lock_guard(*lock_);

      if (role_ == FOLLOWER) {
        if (std::chrono::system_clock::now() - last_heartbeat_ > ElectionTimeout) {
          raft_state_->set_current_term(raft_state_->current_term() + 1);
          role_ = CANDIDATE;
          sent_vote_requests_ = false;
        }
      } else if (role_ == CANDIDATE) {
        last_heartbeat_ = std::chrono::system_clock::now();
        updateLeaderElection();
      } else if (role_ == LEADER) {
        last_heartbeat_ = std::chrono::system_clock::now();
        updateIndices();
        updateFollowers();
      }
      commitEntries();
    }

    auto sleep_for = raft_config_->random->randomDurationBetween(MinSleepTime, MaxSleepTime);
    if (std::chrono::system_clock::now() < current_time + sleep_for)
      std::this_thread::sleep_until(current_time + sleep_for);
    else
      util::logger->msg(util::LogLevel::WARN, "raft worker sleep cycle missed");
  }
}
void Raft::updateLeaderElection() {
  if (!sent_vote_requests_) {
    votes_received_.clear();
    if (extended_peers_->at(raft_config_->me.id())->active()) votes_received_.emplace_back(raft_config_->me.id());
    raft_state_->set_voted_for(raft_config_->me.id());

    protos::raft::RequestVoteRequest request = {};
    request.set_term(raft_state_->current_term());
    request.set_candidate_id(raft_config_->me.id());
    request.set_last_log_index(raft_state_->log_size() - 1);
    request.set_last_log_term(0 < raft_state_->log_size() - raft_state_->log_offset()
                                  ? atLogIndex(request.last_log_index()).term()
                                  : raft_state_->snapshot().last_included_term());

    for (auto &[peer_id, peer] : *extended_peers_) {
      if (!peer->active() || peer_id == raft_config_->me.id()) continue;
      auto call = std::make_shared<
          ExtendedRaftPeer::Call<protos::raft::RequestVoteRequest, protos::raft::RequestVoteResponse>>();
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
      if (!peer->active() || peer_id == raft_config_->me.id()) continue;
      if (!std::holds_alternative<ExtendedRaftPeer::RequestVoteCall>(peer->last_call)) continue;

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
        for (auto &[_, p] : *extended_peers_) peer->last_call = std::monostate();
        return;
      }
    }

    if (checkAllConfigsAgreement(votes_received_)) {
      role_ = LEADER;
      leader_ = raft_config_->me.id();
      for (auto &peer : *raft_state_->mutable_peers()) {
        peer.set_match_index(peer.peer().id() == raft_config_->me.id() ? (raft_state_->log_size() - 1) : 0);
        peer.set_next_index(raft_state_->log_size());
      }
    } else if (remaining_voter_count == 0) {
      role_ = FOLLOWER;
    }
  }
}
void Raft::updateIndices() {
  while (commit_index_ < raft_state_->log_size() && checkAllConfigsAgreement(agreersForIndex(commit_index_ + 1)))
    commit_index_++;
}

void Raft::updateFollowers() {
  for (auto &peer : *raft_state_->mutable_peers()) {
    auto &extended_peer = extended_peers_->at(peer.peer().id());

    if (raft_config_->me.id() == peer.peer().id()) continue;
    if (std::holds_alternative<ExtendedRaftPeer::InstallSnapshotCall>(extended_peer->last_call)) {
      const auto call = std::get<ExtendedRaftPeer::InstallSnapshotCall>(extended_peer->last_call);
      if (!call->complete) continue;
      if (!call->status.ok()) {
        updateFollower(peer, extended_peer);
        continue;
      }

      if (raft_state_->current_term() < call->response.term()) {
        raft_state_->set_current_term(call->response.term());
        leader_ = call->response.leader_id();
        role_ = FOLLOWER;
        return;
      }

      peer.set_snapshot_id(call->request.snapshot_id());
      peer.set_chunk_offset(call->request.chunk_offset() + 1);
      updateFollower(peer, extended_peer);
    } else if (std::holds_alternative<ExtendedRaftPeer::AppendEntriesCall>(extended_peer->last_call)) {
      const auto call = std::get<ExtendedRaftPeer::AppendEntriesCall>(extended_peer->last_call);
      if (!call->complete) continue;
      if (!call->status.ok()) {
        updateFollower(peer, extended_peer);
        continue;
      }

      if (raft_state_->current_term() < call->response.term()) {
        raft_state_->set_current_term(call->response.term());
        leader_ = call->response.leader_id();
        role_ = FOLLOWER;
        return;
      }

      if (call->response.success()) {
        if (!call->request.entries().empty()) {
          auto updated_to = call->request.entries(call->request.entries_size() - 1).index();
          peer.set_match_index(updated_to);
          peer.set_next_index(updated_to + 1);
        }
      } else {
        auto next_index = call->response.conflict_index();
        if (next_index <= raft_state_->snapshot().last_included_index() && call->response.conflict_term() == -1
            || call->response.conflict_term() < raft_state_->snapshot().last_included_term()
                && call->response.conflict_term() != -1) {
          // A snapshot is required
        } else {
          if (call->response.conflict_term() != -1)
            for (auto entry = raft_state_->entries().rbegin(); entry != raft_state_->entries().rend(); ++entry)
              if (entry->term() == call->response.conflict_term()) {
                next_index = entry->index();
                break;
              }

          peer.set_next_index(next_index);
        }
      }

      updateFollower(peer, extended_peer);
    } else {
      updateFollower(peer, extended_peer);
    }
  }
}
void Raft::updateFollower(const protos::raft::RaftState_PeerState &peer,
                          const std::unique_ptr<ExtendedRaftPeer> &extended_peer) {
  grpc::ClientContext client_context = {};
  grpc::Status status;

  if (peer.next_index() < raft_state_->log_offset()) {
    auto call = std::make_shared<
        ExtendedRaftPeer::Call<protos::raft::InstallSnapshotRequest, protos::raft::InstallSnapshotResponse>>();
    extended_peer->last_call = call;

    call->request.set_term(raft_state_->current_term());
    call->request.set_leader_id(raft_config_->me.id());

    call->request.set_snapshot_id(raft_state_->snapshot().snapshot_id());
    if (peer.snapshot_id() != raft_state_->snapshot().snapshot_id()) call->request.set_chunk_offset(0);
    else
      call->request.set_chunk_offset(peer.chunk_offset() + 1);
    fillWithChunk(call->request);
    call->request.set_last_chunk(call->request.chunk_offset() + 1 == raft_state_->snapshot().chunk_count());

    extended_peer->connection->async()->InstallSnapshot(&call->client_context, &call->request, &call->response,
                                                        [call](const grpc::Status &status) {
                                                          call->status = status;
                                                          call->complete = true;
                                                        });
  } else {
    auto call = std::make_shared<
        ExtendedRaftPeer::Call<protos::raft::AppendEntriesRequest, protos::raft::AppendEntriesResponse>>();
    extended_peer->last_call = call;
    call->request.set_term(raft_state_->current_term());
    call->request.set_leader_id(raft_config_->me.id());

    call->request.set_prev_log_index(peer.next_index() - 1);
    call->request.set_prev_log_term(peer.next_index() == raft_state_->log_offset()
                                        ? raft_state_->snapshot().last_included_term()
                                        : atLogIndex(call->request.prev_log_index()).term());

    for (auto i = peer.next_index(); i < raft_state_->log_size(); i++)
      call->request.mutable_entries()->Add()->CopyFrom(atLogIndex(i));
    call->request.set_leader_commit_index(commit_index_);

    extended_peer->connection->async()->AppendEntries(&call->client_context, &call->request, &call->response,
                                                      [call](const grpc::Status &status) {
                                                        call->status = status;
                                                        call->complete = true;
                                                      });
  }
}
const protos::raft::LogEntry &Raft::atLogIndex(LogIndex index) const {
  if (index < raft_state_->log_offset()) throw RaftException(RaftExceptionType::IndexEarlierThanSnapshot);
  if (raft_state_->log_size() <= index) throw RaftException(RaftExceptionType::IndexOutOfLogBounds);
  return raft_state_->entries(static_cast<int>(index - raft_state_->log_offset()));
}
void Raft::commitEntries() {
  while (last_applied_ < commit_index_) {
    last_applied_++;
    auto &entry = atLogIndex(last_applied_);

    switch (entry.log_data().type()) {
      case protos::raft::COMMAND: raft_config_->apply_command(entry); break;
      case protos::raft::CONFIG: {
        if (proposed_configs_.empty()) throw RaftException(RaftExceptionType::ConfigNotInProposedConfig);
        if (entry.index() != proposed_configs_.front())
          throw RaftException(RaftExceptionType::ConfigNotInProposedConfig);

        raft_state_->mutable_base_config()->ParseFromString(entry.log_data().data());
        proposed_configs_.pop_front();
        raft_config_->apply_config_update(entry);
      } break;
      default: throw RaftException(RaftExceptionType::AttemptedCommitOfUnknownEntry);
    }
  }
}
bool Raft::checkAllConfigsAgreement(const std::list<PeerId> &agreers) const {
  if (!hasAgreement(raft_state_->base_config(), agreers)) return false;

  for (const auto &config_index : proposed_configs_) {
    protos::raft::Config config = {};
    config.ParseFromString(atLogIndex(config_index).log_data().data());
    if (!hasAgreement(config, agreers)) return false;
  }

  return true;
}
std::list<PeerId> Raft::agreersForIndex(LogIndex index) const {
  std::list<PeerId> result = {};
  for (const auto &peer : raft_state_->peers())
    if (index <= peer.match_index()) result.emplace_back(peer.peer().id());
  return result;
}
void Raft::fillWithChunk(protos::raft::InstallSnapshotRequest &request) {
  if (raft_state_->snapshot().snapshot_id() != request.snapshot_id())
    throw std::runtime_error("attempted to fill request with old snapshot");

  std::ifstream snapshot_file = {};
  snapshot_file.open(raft_state_->snapshot().file());
  snapshot_file.seekg(static_cast<long long>(request.chunk_offset() * SnapshotChunkSize));

  auto amount_to_read = SnapshotChunkSize;
  if (request.chunk_offset() == raft_state_->snapshot().chunk_count() - 1)
    amount_to_read = raft_state_->snapshot().file_size() % SnapshotChunkSize;

  auto chunk_data = std::make_unique<std::string>();
  chunk_data->reserve(amount_to_read);
  snapshot_file.read(chunk_data->data(), static_cast<long long>(amount_to_read));
  request.set_allocated_chunk(chunk_data.release());
}
void Raft::registerNewConfig(LogIndex log_index, const protos::raft::Config &config, bool base_config) {
  for (auto &config_peer : config.peers()) {
    if (extended_peers_->contains(config_peer.id())) {
      auto &peer = extended_peers_->at(config_peer.id());
      if (!base_config && config_peer.voting()) peer->active_in_configs.emplace_back(log_index);
      if (base_config && config_peer.voting()) peer->active_in_base_config = true;
    } else {
      auto channel = grpc::CreateChannel(config_peer.data().address(), grpc::InsecureChannelCredentials());
      auto stub = protos::raft::Raft::NewStub(channel);
      auto peer = std::make_unique<ExtendedRaftPeer>(std::move(stub));
      if (!base_config && config_peer.voting()) peer->active_in_configs.emplace_back(log_index);
      if (base_config && config_peer.voting()) peer->active_in_base_config = true;
      extended_peers_->emplace(config_peer.id(), std::move(peer));
    }
  }

  if (base_config) raft_state_->mutable_base_config()->CopyFrom(config);
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
    registerNewConfig(log_index, config, false);
  }

  for (auto &peer : *raft_state_->mutable_peers())
    if (peer.peer().id() == raft_config_->me.id()) peer.set_match_index(peer.match_index() + 1);

  persist();
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
        : atLogIndex(raft_state_->log_size() - 1).term();

    if (conflict_term != request->prev_log_term()) {
      response->set_conflict_term(conflict_term);
      auto i = request->prev_log_index();
      while (raft_state_->log_offset() < i && atLogIndex(i).term() != request->prev_log_term()) i--;
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
  for (const auto &entry : request->entries()) {
    raft_state_->mutable_entries()->Add()->CopyFrom(entry);
    raft_state_->set_log_size(raft_state_->log_size() + 1);
  }

  persist();
  response->set_success(true);
  return grpc::Status::OK;
}
grpc::Status Raft::RequestVote(::grpc::ServerContext *context, const ::protos::raft::RequestVoteRequest *request,
                               ::protos::raft::RequestVoteResponse *response) {
  std::lock_guard<std::mutex> lock_guard(*lock_);

  response->set_term(raft_state_->current_term());

  auto last_log_index = raft_state_->log_size() - 1;
  auto last_log_term = 0 < raft_state_->log_size() - raft_state_->log_size()
      ? atLogIndex(last_log_index).term()
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

  persist();
  return grpc::Status::OK;
}
grpc::Status Raft::InstallSnapshot(::grpc::ServerContext *context,
                                   const ::protos::raft::InstallSnapshotRequest *request,
                                   ::protos::raft::InstallSnapshotResponse *response) {
  std::lock_guard<std::mutex> lock_guard(*lock_);

  persist();
  return grpc::Status::OK;
}

void Raft::persist() {
  if (!raft_config_->snapshot_file.has_value()) return;

  std::ofstream persistent_file = {};
  persistent_file.open(raft_config_->snapshot_file.value());
  raft_state_->SerializeToOstream(&persistent_file);
  auto written_count = persistent_file.tellp();
  persistent_file.close();

  raft_config_->on_persist(written_count);
}

RaftClient::RaftClient(PeerId me, const protos::raft::Config &config) : me_(std::move(me)) { config_.CopyFrom(config); }

void RaftClient::updateConfig(const protos::raft::Config &config) {
  std::unique_lock<std::shared_mutex> write_lock(*lock_);
  config_.CopyFrom(config);
  cached_connection_info_ = std::nullopt;
}

LogIndex RaftClient::start(const std::string &command) {
  protos::raft::StartRequest request = {};
  request.mutable_log_data()->set_data(command);
  request.mutable_log_data()->set_type(protos::raft::COMMAND);
  return doRequest(request);
}
LogIndex RaftClient::startConfig(const protos::raft::Config &config) {
  protos::raft::StartRequest request = {};
  request.mutable_log_data()->set_data(config.SerializeAsString());
  request.mutable_log_data()->set_type(protos::raft::CONFIG);
  return doRequest(request);
}

LogIndex RaftClient::doRequest(const protos::raft::StartRequest &request) {
  std::shared_lock<std::shared_mutex> lock(*lock_);

  checkForCacheExistence(lock);
  auto &[leader_id, stub] = cached_connection_info_.value();

  auto time = std::chrono::system_clock::now();
  std::chrono::time_point<std::chrono::system_clock, std::chrono::system_clock::duration> last_call;
  protos::raft::StartResponse response = {};
  grpc::Status status = {};
  do {
    grpc::ClientContext client_context;
    last_call = std::chrono::system_clock::now();
    auto deadline = last_call + StartRequestBuffer;
    client_context.set_deadline(deadline);

    status = stub->Start(&client_context, request, &response);

    if (!status.ok()) updateCache(lock, me_);
    else if (status.ok() && response.data_case() == protos::raft::StartResponse::DataCase::kLeaderId)
      updateCache(lock, response.leader_id());

    if (std::chrono::system_clock::now() - time > StartRequestTimeout) {
      throw std::runtime_error("start request timeout");
    }

    if (std::chrono::system_clock::now() < deadline) std::this_thread::sleep_until(deadline);
  } while (!status.ok() || response.data_case() != protos::raft::StartResponse::DataCase::kLogIndex);

  return response.log_index();
}
void RaftClient::checkForCacheExistence(std::shared_lock<std::shared_mutex> &lock) {
  if (cached_connection_info_.has_value()) return;

  lock.unlock();
  {
    std::unique_lock<std::shared_mutex> write_lock(*lock_);
    if (cached_connection_info_.has_value()) {
      write_lock.unlock();
      lock.lock();
      return;
    }

    auto &peer = config_.peers(0);
    auto channel = grpc::CreateChannel(peer.data().address(), grpc::InsecureChannelCredentials());
    auto stub = protos::raft::Raft::NewStub(channel);
    cached_connection_info_ = {peer.id(), std::move(stub)};
  }

  lock.lock();
}
void RaftClient::updateCache(std::shared_lock<std::shared_mutex> &lock, const std::string &leader_id) {
  lock.unlock();

  {
    std::unique_lock<std::shared_mutex> write_lock(*lock_);

    if (cached_connection_info_->first == leader_id) {
      write_lock.unlock();
      lock.lock();
      return;
    }

    for (auto &peer : config_.peers()) {
      if (peer.id() == leader_id) {
        auto channel = grpc::CreateChannel(peer.data().address(), grpc::InsecureChannelCredentials());
        auto stub = protos::raft::Raft::NewStub(channel);
        cached_connection_info_ = {peer.id(), std::move(stub)};
        write_lock.unlock();
        lock.lock();
        return;
      }
    }
  }

  lock.lock();
  throw std::runtime_error("no such peer");
}

}// namespace flashpoint::raft