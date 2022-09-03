#include "raft/raft.hpp"

namespace flashpoint::raft {

bool hasAgreement(const protos::raft::Config &config, const std::list<PeerId> &agreers) {
  std::cout << "config " << config.peers_size() << " agreers " << agreers.size() << std::endl;

  auto agree_count = 0;
  for (const auto &peer : agreers) {
    for (const auto &config_peer : config.peers())
      if (config_peer.id() == peer) agree_count++;
  }
  std::cout << "agreement " << (agree_count > (config.peers_size() / 2)) << std::endl;
  return agree_count > (config.peers_size() / 2);
}

RaftPeer::RaftPeer(protos::raft::Peer peer, std::shared_ptr<protos::raft::Raft::StubInterface> connection)
    : peer(std::move(peer)), connection(std::move(connection)) {}
const PeerId &RaftPeer::peerId() const { return peer.id(); }
bool RaftPeer::active() { return active_in_base_config || !active_in_configs.empty(); }

Raft::Raft(std::unique_ptr<RaftConfig> config, const protos::raft::RaftState &save_state)
    : raft_config_(std::move(config)) {
  std::cout << "started and i am " << raft_config_->me.id() << std::endl;
  registerNewConfig(0, raft_config_->starting_config, true);
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

  protos::raft::Snapshot snapshot = {};
  snapshot.set_snapshot_id(snapshot_.snapshot_id() + 1);
  snapshot.set_last_included_index(included_index);
  snapshot.set_last_included_term(atLogIndex(included_index).term());

  snapshot.set_file_size(std::filesystem::file_size(std::filesystem::path(snapshot_file)));
  snapshot.set_file(std::move(snapshot_file));
  snapshot.set_chunk_count((snapshot.file_size() / 1000000 / 256) + 1);

  log_.erase(log_.begin(), log_.begin() + static_cast<int>(included_index - log_offset_));

  snapshot_ = std::move(snapshot);
  return true;
}

void Raft::worker() {
  while (running_) {
    auto current_time = std::chrono::system_clock::now();

    {
      std::lock_guard<std::mutex> lock_guard(*lock_);
      std::cout << "worker running " << role_ << " "
                << (std::chrono::system_clock::now() - last_heartbeat_ > ElectionTimeout) << std::endl;

      if (role_ == FOLLOWER) {
        std::cout << "i am a follower" << std::endl;
        if (std::chrono::system_clock::now() - last_heartbeat_ > ElectionTimeout) {
          current_term_++;
          role_ = CANDIDATE;
          sent_vote_requests_ = false;
          std::cout << "became candidate" << std::endl;
        }
      } else if (role_ == CANDIDATE) {
        std::cout << "i am a candidate" << std::endl;
        updateLeaderElection();
      } else if (role_ == LEADER) {
        std::cout << "i am a leader" << std::endl;
        last_heartbeat_ = std::chrono::system_clock::now();
        updateIndices();
        updateFollowers();
      }
      commitEntries();
    }

    auto sleep_for = raft_config_->random_->randomDurationBetween(MinSleepTime, MaxSleepTime);
    if (std::chrono::system_clock::now() < current_time + sleep_for)
      std::this_thread::sleep_until(current_time + sleep_for);
    else
      util::logger->msg(util::LogLevel::WARN, "raft worker sleep cycle missed");
  }
}
void Raft::updateLeaderElection() {
  if (!sent_vote_requests_) {
    votes_received_.clear();
    if (getPeer(raft_config_->me.id()).active()) votes_received_.emplace_back(raft_config_->me.id());

    protos::raft::RequestVoteRequest request = {};
    request.set_term(current_term_);
    request.set_candidate_id(raft_config_->me.id());
    request.set_last_log_index(log_size_ - 1);
    request.set_last_log_term(0 < log_size_ - log_offset_ ? atLogIndex(request.last_log_index()).term()
                                                          : snapshot_.last_included_term());

    for (const auto &peer : peers_) {
      std::cout << "peer: " << peer->peerId() << " is active: " << peer->active()
                << " is me: " << (peer->peerId() == raft_config_->me.id()) << std::endl;
      if (!peer->active() || peer->peerId() == raft_config_->me.id()) continue;
      auto call =
          std::make_shared<RaftPeer::Call<protos::raft::RequestVoteRequest, protos::raft::RequestVoteResponse>>();
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

    for (const auto &peer : peers_) {
      if (!peer->active() || peer->peerId() == raft_config_->me.id()) continue;
      if (!std::holds_alternative<RaftPeer::RequestVoteCall>(peer->last_call)) continue;

      auto &call = std::get<RaftPeer::RequestVoteCall>(peer->last_call);
      if (!call->complete) {
        remaining_voter_count++;
        continue;
      }
      auto &response = call->response;

      if (response.vote_granted()) {
        votes_received_.emplace_back(peer->peerId());
      } else if (current_term_ < response.term()) {
        current_term_ = response.term();
        leader_ = response.leader_id();
        role_ = FOLLOWER;
        for (const auto &p : peers_) p->last_call = std::monostate();
        return;
      }
    }

    if (checkAllConfigsAgreement(votes_received_)) {
      std::cout << "won election" << std::endl;
      role_ = LEADER;
      for (const auto &p : peers_) {
        p->match_index = 0;
        p->next_index = log_size_;
      }
    } else if (remaining_voter_count == 0) {
      std::cout << "lost election" << std::endl;
      role_ = FOLLOWER;
    }
  }
}
void Raft::updateIndices() {
  while (commit_index_ < log_size_) {
    if (checkAllConfigsAgreement(agreersForIndex(commit_index_ + 1))) return;
    commit_index_++;
  }
}

void Raft::updateFollowers() {
  for (const auto &peer : peers_) {
    if (raft_config_->me.id() == peer->peerId()) continue;
    auto peer_id = peer->peerId();
    if (std::holds_alternative<std::monostate>(peer->last_call)
        || std::holds_alternative<RaftPeer::RequestVoteCall>(peer->last_call))
      updateFollower(peer);
    else if (std::holds_alternative<RaftPeer::InstallSnapshotCall>(peer->last_call)) {
      const auto call = std::get<RaftPeer::InstallSnapshotCall>(peer->last_call);
      if (!call->complete) continue;
      if (!call->status.ok()) {
        updateFollower(peer);
        continue;
      }

      if (current_term_ < call->response.term()) {
        current_term_ = call->response.term();
        role_ = FOLLOWER;
        return;
      }

      peer->snapshot_id = call->request.snapshot_id();
      peer->chunk_offset = call->request.chunk_offset() + 1;
      updateFollower(peer);
    } else if (std::holds_alternative<RaftPeer::AppendEntriesCall>(peer->last_call)) {
      const auto call = std::get<RaftPeer::AppendEntriesCall>(peer->last_call);
      if (!call->complete) continue;
      if (!call->status.ok()) {
        updateFollower(peer);
        continue;
      }

      if (current_term_ < call->response.term()) {
        current_term_ = call->response.term();
        role_ = FOLLOWER;
        return;
      }

      if (call->response.success()) {
        if (!call->request.entries().empty()) {
          auto updated_to = call->request.entries().end()->index();
          peer->match_index = updated_to;
          peer->next_index = updated_to + 1;
        }
      } else {
        auto next_index = call->response.conflict_index();
        if (next_index <= snapshot_.last_included_index() && call->response.conflict_term() == -1
            || call->response.conflict_term() < snapshot_.last_included_term()
                && call->response.conflict_term() != -1) {
          // A snapshot is required
          updateFollower(peer);
          return;
        }

        if (call->response.conflict_term() != -1)
          for (auto entry = log_.rbegin(); entry != log_.rend(); ++entry)
            if (entry->term() == call->response.conflict_term()) {
              next_index = entry->index();
              break;
            }

        peer->next_index = next_index;
        updateFollower(peer);
      }
    }
  }
}
void Raft::updateFollower(const std::unique_ptr<RaftPeer> &peer) {
  grpc::ClientContext client_context = {};
  grpc::Status status;


  if (peer->next_index < log_offset_) {
    auto call =
        std::make_shared<RaftPeer::Call<protos::raft::InstallSnapshotRequest, protos::raft::InstallSnapshotResponse>>();
    call->request.set_term(current_term_);
    call->request.set_leader_id(raft_config_->me.id());

    call->request.set_snapshot_id(snapshot_.snapshot_id());
    if (peer->snapshot_id != snapshot_.snapshot_id()) call->request.set_chunk_offset(0);
    else
      call->request.set_chunk_offset(peer->chunk_offset + 1);
    fillWithChunk(call->request);
    call->request.set_last_chunk(call->request.chunk_offset() + 1 == snapshot_.chunk_count());

    peer->connection->async()->InstallSnapshot(&call->client_context, &call->request, &call->response,
                                               [call](const grpc::Status &status) {
                                                 call->status = status;
                                                 call->complete = true;
                                               });
  } else {
    auto call =
        std::make_shared<RaftPeer::Call<protos::raft::AppendEntriesRequest, protos::raft::AppendEntriesResponse>>();
    call->request.set_term(current_term_);
    call->request.set_leader_id(raft_config_->me.id());

    call->request.set_prev_log_index(peer->next_index - 1);
    call->request.set_prev_log_term(peer->next_index == log_offset_
                                        ? snapshot_.last_included_term()
                                        : atLogIndex(call->request.prev_log_index()).term());

    for (auto entry = log_.begin() + static_cast<int>(peer->next_index - log_offset_); entry != log_.end(); ++entry)
      call->request.mutable_entries()->Add()->CopyFrom(*entry);

    call->request.set_leader_commit_index(commit_index_);

    peer->connection->async()->AppendEntries(&call->client_context, &call->request, &call->response,
                                             [call](const grpc::Status &status) {
                                               call->status = status;
                                               call->complete = true;
                                             });
  }
}
const protos::raft::LogEntry &Raft::atLogIndex(LogIndex index) {
  if (index < log_offset_) throw RaftException(RaftExceptionType::IndexEarlierThanSnapshot);
  if (log_size_ <= index) throw RaftException(RaftExceptionType::IndexOutOfLogBounds);
  return log_[index - log_offset_];
}
void Raft::commitEntries() {
  while (last_applied_ < commit_index_) {
    auto &entry = atLogIndex(last_applied_);
    switch (entry.log_data().type()) {
      case protos::raft::COMMAND: raft_config_->apply_command(entry); break;
      case protos::raft::CONFIG: {
        if (proposed_configs_.empty()) throw RaftException(RaftExceptionType::ConfigNotInProposedConfig);
        if (entry.index() != proposed_configs_.front())
          throw RaftException(RaftExceptionType::ConfigNotInProposedConfig);

        protos::raft::Config config = {};
        config.ParseFromString(entry.log_data().data());
        base_config_ = config;
        proposed_configs_.pop_front();
        raft_config_->apply_config_update(entry);
      } break;
      default: throw RaftException(RaftExceptionType::AttemptedCommitOfUnknownEntry);
    }

    last_applied_++;
  }
}
bool Raft::checkAllConfigsAgreement(const std::list<PeerId> &agreers) {
  if (!hasAgreement(base_config_, agreers)) return false;

  for (const auto &config_index : proposed_configs_) {
    protos::raft::Config config = {};
    config.ParseFromString(atLogIndex(config_index).log_data().data());
    if (!hasAgreement(config, agreers)) return false;
  }

  return true;
}
std::list<PeerId> Raft::agreersForIndex(LogIndex index) {
  std::list<PeerId> result = {};
  for (const auto &peer : peers_)
    if (index <= peer->match_index) result.emplace_back(peer->peerId());
  return result;
}
void Raft::fillWithChunk(protos::raft::InstallSnapshotRequest &request) {
  if (snapshot_.snapshot_id() != request.snapshot_id())
    throw std::runtime_error("attempted to fill request with old snapshot");

  std::ifstream snapshot_file = {};
  snapshot_file.open(snapshot_.file());
  snapshot_file.seekg(static_cast<long long>(request.chunk_offset() * SnapshotChunkSize));

  auto amount_to_read = SnapshotChunkSize;
  if (request.chunk_offset() == snapshot_.chunk_count() - 1) amount_to_read = snapshot_.file_size() % SnapshotChunkSize;

  auto chunk_data = std::make_unique<std::string>();
  chunk_data->reserve(amount_to_read);
  snapshot_file.read(chunk_data->data(), static_cast<long long>(amount_to_read));
  request.set_allocated_chunk(chunk_data.release());
}

grpc::Status Raft::Start(::grpc::ServerContext *context, const ::protos::raft::StartRequest *request,
                         ::protos::raft::StartResponse *response) {
  std::lock_guard<std::mutex> lock_guard(*lock_);
  if (role_ != LEADER) {
    response->set_leader_id(leader_);
    return grpc::Status::OK;
  }

  protos::raft::LogEntry entry = {};
  LogIndex log_index = log_size_++;
  entry.set_index(log_index);
  entry.set_term(current_term_);
  entry.mutable_log_data()->CopyFrom(request->log_data());
  entry.set_data_valid(false);

  if (request->log_data().type() == protos::raft::LogDataType::CONFIG) {
    protos::raft::Config config = {};
    config.ParseFromString(entry.log_data().data());
    registerNewConfig(log_index, config, false);
  }

  log_.emplace_back(std::move(entry));

  response->set_log_index(log_index);
  return grpc::Status::OK;
}
grpc::Status Raft::AppendEntries(::grpc::ServerContext *context, const ::protos::raft::AppendEntriesRequest *request,
                                 ::protos::raft::AppendEntriesResponse *response) {
  std::lock_guard<std::mutex> lock_guard(*lock_);

  response->set_term(current_term_);
  if (request->term() < current_term_) {
    response->set_success(true);
    return grpc::Status::OK;
  } else if (current_term_ < request->term()) {
    current_term_ = request->term();
    role_ = FOLLOWER;
  }

  auto last_log_index = log_size_ - 1;

  if (last_log_index < request->prev_log_index()) {
    response->set_conflict_index(last_log_index + 1);
    response->set_conflict_term(-1);
    response->set_success(false);
    return grpc::Status::OK;
  } else {
    auto conflict_term = atLogIndex(request->prev_log_index()).term();
    if (conflict_term != request->prev_log_term()) {
      response->set_conflict_term(conflict_term);
      auto i = request->prev_log_index();
      while (log_offset_ < i && atLogIndex(i).term() != request->prev_log_term()) i--;
      response->set_conflict_index(i);
      response->set_success(false);
      return grpc::Status::OK;
    }
  }

  if (request->prev_log_index() < log_size_ - 1)
    log_.erase(log_.begin() + static_cast<int>(request->prev_log_index() - log_offset_ + 1), log_.end());

  last_log_index = log_offset_ - 1;

  commit_index_ = std::min(request->leader_commit_index(), last_log_index);
  last_heartbeat_ = std::chrono::system_clock::now();
  for (const auto &entry : request->entries()) log_.emplace_back(entry);//TODO: not before commit index?

  return grpc::Status::OK;
}
grpc::Status Raft::RequestVote(::grpc::ServerContext *context, const ::protos::raft::RequestVoteRequest *request,
                               ::protos::raft::RequestVoteResponse *response) {
  std::lock_guard<std::mutex> lock_guard(*lock_);
  std::cout << "received request vote from " << request->candidate_id() << std::endl;

  response->set_term(current_term_);

  auto last_log_index = log_size_ - 1;
  auto last_log_term = 0 < log_size_ - log_offset_ ? atLogIndex(last_log_index).term() : snapshot_.last_included_term();

  if (current_term_ < request->term()) {
    current_term_ = request->term();
    role_ = FOLLOWER;
    voted_for_ = std::nullopt;
  }

  if (response->term() < current_term_ || (voted_for_.has_value() && voted_for_.value() != request->candidate_id())
      || (request->last_log_term() < last_log_term)
      || (request->last_log_term() == last_log_term && request->last_log_index() < last_log_index)) {
    response->set_vote_granted(false);
    std::cout << "not granted vote" << std::endl;
    return grpc::Status::OK;
  }

  std::cout << "granted vote" << std::endl;
  last_heartbeat_ = std::chrono::system_clock::now();
  response->set_vote_granted(true);
  voted_for_ = request->candidate_id();

  return grpc::Status::OK;
}
grpc::Status Raft::InstallSnapshot(::grpc::ServerContext *context,
                                   const ::protos::raft::InstallSnapshotRequest *request,
                                   ::protos::raft::InstallSnapshotResponse *response) {
  std::lock_guard<std::mutex> lock_guard(*lock_);
  return grpc::Status::OK;
}
RaftPeer &Raft::getPeer(const PeerId &peer_id) {
  for (auto &peer : peers_) {
    std::cout << "searching for " << peer_id << " and current peer is " << peer->peerId() << std::endl;
    if (peer->peerId() == peer_id) return *peer;
  }
  throw std::runtime_error("no such peer");
}
void Raft::registerNewConfig(LogIndex log_index, const protos::raft::Config &config, bool base_config) {
  for (auto &config_peer : config.peers()) {
    try {
      auto peer = getPeer(config_peer.id());
      if (!base_config && config_peer.voting()) peer.active_in_configs.emplace_back(log_index);
      if (base_config && config_peer.voting()) peer.active_in_base_config = true;
    } catch (const std::runtime_error &) {
      auto channel = grpc::CreateChannel(config_peer.data().address(), grpc::InsecureChannelCredentials());
      auto stub = protos::raft::Raft::NewStub(channel);
      auto peer = std::make_unique<RaftPeer>(config_peer, std::move(stub));
      if (!base_config && config_peer.voting()) peer->active_in_configs.emplace_back(log_index);
      if (base_config && config_peer.voting()) peer->active_in_base_config = true;
      peers_.emplace_back(std::move(peer));
    }
  }

  if (base_config) base_config_.CopyFrom(config);
}

RaftClient::RaftClient(const protos::raft::Config &config) { config_.CopyFrom(config); }

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
  std::cout << "doing request" << std::endl;

  checkForCacheExistence(lock);
  auto &[leader_id, stub] = cached_connection_info_.value();

  auto time = std::chrono::system_clock::now();
  protos::raft::StartResponse response = {};
  grpc::Status status = {};
  do {
    grpc::ClientContext client_context;
    std::cout << "making request to " << leader_id << std::endl;
    status = stub->Start(&client_context, request, &response);

    if (status.ok() && response.data_case() == protos::raft::StartResponse::DataCase::kLeaderId)
      updateCache(lock, response.leader_id());

    if (std::chrono::system_clock::now() - time > StartRequestTimeout)
      throw std::runtime_error("start request timeout");
  } while (!status.ok() || response.data_case() != protos::raft::StartResponse::DataCase::kLogIndex);

  return response.log_index();
}
void RaftClient::checkForCacheExistence(std::shared_lock<std::shared_mutex> &lock) {
  if (cached_connection_info_.has_value()) return;

  std::cout << "checking cache" << std::endl;
  lock.unlock();
  std::cout << "checking cache unlock" << std::endl;
  {
    std::unique_lock<std::shared_mutex> write_lock(*lock_);
    if (cached_connection_info_.has_value()) {
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

  bool updated = true;
  {
    std::unique_lock<std::shared_mutex> write_lock(*lock_);
    if (cached_connection_info_->first == leader_id) {
      lock.lock();
      return;
    }

    for (auto &peer : config_.peers()) {
      if (peer.id() == leader_id) {
        auto channel = grpc::CreateChannel(peer.data().address(), grpc::InsecureChannelCredentials());
        auto stub = protos::raft::Raft::NewStub(channel);
        cached_connection_info_ = {peer.id(), std::move(stub)};
        updated = true;
        break;
      }
    }
  }

  lock.lock();
  if (!updated) throw std::runtime_error("no such peer");
}

}// namespace flashpoint::raft