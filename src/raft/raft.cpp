#include "raft/raft.hpp"

namespace flashpoint::raft {

bool hasAgreement(const protos::raft::Config &config, const std::list<PeerId> &agreers) {
  auto agree_count = 0;
  for (const auto &peer : agreers)
    for (const auto &config_peer : config.peers())
      if (config_peer.id() == peer) agree_count++;
  return agree_count > (config.peers_size() / 2);
}

std::unique_ptr<RaftPeer> connectToPeer(protos::raft::Peer peer_data) {
  auto channel = grpc::CreateChannel(peer_data.data().address(), grpc::InsecureChannelCredentials());
  return std::make_unique<RaftPeer>(std::move(peer_data), std::make_shared<protos::raft::Raft::Stub>(channel));
}

RaftPeer::RaftPeer(protos::raft::Peer peer, std::shared_ptr<protos::raft::Raft::StubInterface> connection)
    : peer(std::move(peer)), connection(std::move(connection)) {}
const PeerId &RaftPeer::peerId() const { return peer.id(); }

Raft::Raft(std::unique_ptr<RaftConfig> config, const protos::raft::RaftState &save_state)
    : raft_config_(std::move(config)) {}

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

      if (role_ == FOLLOWER) {
        if (std::chrono::system_clock::now() - last_heartbeat_ > ElectionTimeout) {
          auto new_term = current_term_ + 1;
          current_term_++;
          role_ = CANDIDATE;
          sent_vote_requests_ = false;
        }
      } else if (role_ == CANDIDATE) {
        updateLeaderElection();
      } else if (role_ == LEADER) {
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
      util::LOGGER->msg(util::LogLevel::WARN, "raft worker sleep cycle missed");
  }
}
void Raft::updateLeaderElection() {
  if (!sent_vote_requests_) {
    protos::raft::RequestVoteRequest request = {};
    votes_received_.clear();

    for (const auto &peer : peers_) {
      if (!peer->active_in_configs.empty()) {
        auto call = RaftPeer::RequestVoteCall();
        peer->last_call = call;
        call->request = request;
        peer->connection->async()->RequestVote(&call->client_context, &call->request, &call->response,
                                               [call](const grpc::Status &status) {
                                                 call->status = status;
                                                 call->complete = true;
                                               });
      }
    }
    sent_vote_requests_ = true;
  } else {
    auto remaining_voter_count = 0;

    for (const auto &peer : peers_) {
      if (peer->active_in_configs.empty()) continue;
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
      role_ = LEADER;
      for (const auto &p : peers_) {
        p->match_index = 0;
        p->next_index = log_size_;
      }
    } else if (remaining_voter_count == 0) {
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
    if (me_ == peer->peerId()) continue;
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
  if (peer->snapshot_id != snapshot_.snapshot_id() || peer->chunk_offset != snapshot_.chunk_count()) {
    auto call = RaftPeer::InstallSnapshotCall();
    call->request.set_term(current_term_);
    call->request.set_leader_id(me_);

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
    auto call = RaftPeer::AppendEntriesCall();
    call->request.set_term(current_term_);
    call->request.set_leader_id(me_);

    call->request.set_prev_log_index(peer->next_index - 1);
    if (log_offset_ == peer->next_index) call->request.set_prev_log_term(atLogIndex(peer->next_index).term());
    else
      call->request.set_prev_log_term(atLogIndex(peer->next_index - 1).term());

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

  log_.emplace_back(std::move(entry));

  response->set_log_index(log_index);
  return grpc::Status::OK;
}
grpc::Status Raft::AppendEntries(::grpc::ServerContext *context, const ::protos::raft::AppendEntriesRequest *request,
                                 ::protos::raft::AppendEntriesResponse *response) {
  return Service::AppendEntries(context, request, response);
}
grpc::Status Raft::RequestVote(::grpc::ServerContext *context, const ::protos::raft::RequestVoteRequest *request,
                               ::protos::raft::RequestVoteResponse *response) {
  return Service::RequestVote(context, request, response);
}
grpc::Status Raft::InstallSnapshot(::grpc::ServerContext *context,
                                   const ::protos::raft::InstallSnapshotRequest *request,
                                   ::protos::raft::InstallSnapshotResponse *response) {
  return Service::InstallSnapshot(context, request, response);
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

  checkForCacheExistence(lock);
  auto &[leader_id, stub] = cached_connection_info_.value();

  auto time = std::chrono::system_clock::now();
  protos::raft::StartResponse response = {};
  grpc::Status status = {};
  do {
    grpc::ClientContext client_context;
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

  lock.unlock();
  std::unique_lock<std::shared_mutex> write_lock(*lock_);
  if (cached_connection_info_.has_value()) {
    lock.lock();
    return;
  }

  auto &peer = config_.peers(0);
  auto channel = grpc::CreateChannel(peer.data().address(), grpc::InsecureChannelCredentials());
  auto stub = protos::raft::Raft::NewStub(channel);
  cached_connection_info_ = {peer.id(), std::move(stub)};

  lock.lock();
}
void RaftClient::updateCache(std::shared_lock<std::shared_mutex> &lock, const std::string &leader_id) {
  lock.unlock();
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

      lock.lock();
      return;
    }
  }

  lock.lock();
  throw std::runtime_error("no such peer");
}

}// namespace flashpoint::raft