#include "raft/raft.hpp"
#include "protos/raft.pb.h"
#include <algorithm>

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
  return std::make_unique<RaftPeer>(std::move(peer_data), std::move(channel));
}

RaftPeer::RaftPeer(protos::raft::Peer peer, std::shared_ptr<protos::raft::Raft::StubInterface> connection)
    : peer(std::move(peer)), connection(std::move(connection)) {}
const PeerId &RaftPeer::peerId() const { return peer.id(); }

Raft::Raft(const RaftConfig &config, const protos::raft::RaftState &save_state)
    : raft_config_(std::make_unique<RaftConfig>(config)) {}

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

  SnapshotId chunk_count = (std::filesystem::file_size(std::filesystem::path(snapshot_file)) / 1000000 / 256) + 1;
  protos::raft::Snapshot snapshot = {};
  snapshot.set_snapshot_id(snapshot_.snapshot_id() + 1);
  snapshot.set_last_included_index(included_index);
  snapshot.set_file(std::move(snapshot_file));
  snapshot.set_chunk_count(chunk_count);
  snapshot_ = std::move(snapshot);
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
          util::THREAD_POOL->newTask([this, new_term]() -> void { leaderElection(current_term_); });// TODO: fix
        }
      } else if (role_ == LEADER) {
        last_heartbeat_ = std::chrono::system_clock::now();
        updateIndices();
        updateFollowers();
      }
      commitEntries();
    }

    auto sleep_for = random_.generateDurationBetween(MinSleepTime, MaxSleepTime);
    if (std::chrono::system_clock::now() < current_time + sleep_for)
      std::this_thread::sleep_until(current_time + sleep_for);
    else
      util::LOGGER->msg(util::LogLevel::WARN, "raft worker sleep cycle missed");
  }
}
void Raft::leaderElection(LogIndex expected_term) {
  std::lock_guard<std::mutex> lock_guard(*lock_);
  if (expected_term != current_term_) return;
}
void Raft::updateIndices() {
  while (commit_index_ < log_size_) {
    if (!hasAgreement(base_config_, agreersForIndex(commit_index_ + 1))) return;

    for (const auto &config_index : proposed_configs_) {
      protos::raft::Config config = {};
      config.ParseFromString(atLogIndex(config_index).base.data());
      if (!hasAgreement(config, agreersForIndex(commit_index_ + 1))) return;
    }
  }
}

void Raft::updateFollowers() {
  for (const auto &peer : peers_) {
    if (me_ == peer.peerId()) continue;
    auto peer_id = peer.peerId();
    util::THREAD_POOL->newTask([=]() { updateFollower(peer_id); });
  }
}
void Raft::updateFollower(const PeerId &peer_id) {
  lock_->lock();
  auto &peer = raftPeerWithId(peer_id);
  auto current_term = current_term_;
  peer.lock->lock();
  lock_->unlock();

  grpc::ClientContext client_context = {};
  grpc::Status status;
  if (peer.snapshot_id != snapshot_.snapshot_id || peer.chunk_offset != snapshot_.chunk_count) {
    protos::raft::InstallSnapshotRequest request = {};


    protos::raft::InstallSnapshotResponse response = {};
    status = peer.connection->InstallSnapshot(&client_context, request, &response);
    if (status.ok()) {
      if (response.term() != current_term) {
        respectNewLeader(response.leader_id(), response.term());
        peer.lock->unlock();
        return;
      } else {
        peer.snapshot_id = request.snapshot_id();
        peer.chunk_offset = request.chunk_offset();
      }
    }
  } else {
    protos::raft::AppendEntriesRequest request = {};


    protos::raft::AppendEntriesResponse response = {};
    status = peer.connection->AppendEntries(&client_context, request, &response);
    if (status.ok()) {
      if (response.term() != current_term) {
        respectNewLeader(response.leader_id(), response.term());
        peer.lock->unlock();
        return;
      } else if (response.success()) {

      } else {
      }
    }
  }

  peer.lock->unlock();
}
const Raft::ExtendedLogEntry &Raft::atLogIndex(LogIndex index) {
  if (index < log_offset_) throw RaftException(RaftExceptionType::IndexEarlierThanSnapshot);
  if (log_size_ <= index) throw RaftException(RaftExceptionType::IndexOutOfLogBounds);
  return log_[index - log_offset_];
}
void Raft::commitEntries() {
  while (last_applied_ < commit_index_) {
    auto &entry = atLogIndex(last_applied_);
    switch (entry.base.type()) {
      case protos::raft::CMD: apply_command_(entry.base); break;
      case protos::raft::CONFIG: {
        if (proposed_configs_.empty()) throw RaftException(RaftExceptionType::ConfigNotInProposedConfig);
        if (entry.base.index() != proposed_configs_.front())
          throw RaftException(RaftExceptionType::ConfigNotInProposedConfig);

        protos::raft::Config config = {};
        config.ParseFromString(entry.base.data());
        base_config_ = config;
        proposed_configs_.pop_front();
        apply_config_update_(entry.base);
      } break;
      default: throw RaftException(RaftExceptionType::AttemptedCommitOfUnknownEntry);
    }
    entry.fulfillment_status->set_value(true);
    last_applied_++;
  }
}
std::list<PeerId> Raft::agreersForIndex(LogIndex index) {
  std::list<PeerId> result = {};
  for (const auto &peer : peers_)
    if (index <= peer.match_index) result.emplace_back(peer.peerId());
  return result;
}
Raft::RaftPeer &Raft::raftPeerWithId(const std::string &id) {
  for (auto &peer : peers_)
    if (peer.peerId() == id) return peer;
  throw RaftException(RaftExceptionType::NoSuchPeerWithId);
}
void Raft::respectNewLeader(const std::string &leader_id, LogTerm term) {
  std::lock_guard<std::mutex> lock_guard(*lock_);
  current_term_ = term;
  leader_ = leader_id;
}
void Raft::updateSnapshot(protos::raft::Snapshot &&snapshot) {}

grpc::ServerWriteReactor<protos::raft::StartResponse> *Raft::Start(::grpc::CallbackServerContext *context,
                                                                   const ::protos::raft::StartRequest *request) {

  if (role_ != LEADER) throw std::runtime_error("can not start on non-leader");

  ExtendedLogEntry entry = {};
  entry.base.set_index(log_size_++);
  entry.base.set_term(current_term_);
  entry.base.set_data(config.SerializeAsString());
  entry.base.set_type(protos::raft::CONFIG);
  entry.base.set_command_valid(false);

  { std::lock_guard<std::mutex> lock_guard(*lock_); }

  return WithCallbackMethod_Start::Start(context, request);
}
grpc::ServerUnaryReactor *Raft::AppendEntries(::grpc::CallbackServerContext *context,
                                              const ::protos::raft::AppendEntriesRequest *request,
                                              ::protos::raft::AppendEntriesResponse *response) {
  auto reactor = context->DefaultReactor();
  reactor->Finish(grpc::Status::OK);
  return reactor;
}
grpc::ServerUnaryReactor *Raft::RequestVote(::grpc::CallbackServerContext *context,
                                            const ::protos::raft::RequestVoteRequest *request,
                                            ::protos::raft::RequestVoteResponse *response) {
  auto reactor = context->DefaultReactor();
  reactor->Finish(grpc::Status::OK);
  return reactor;
}
grpc::ServerUnaryReactor *Raft::InstallSnapshot(::grpc::CallbackServerContext *context,
                                                const ::protos::raft::InstallSnapshotRequest *request,
                                                ::protos::raft::InstallSnapshotResponse *response) {
  auto reactor = context->DefaultReactor();
  reactor->Finish(grpc::Status::OK);
  return reactor;
}

RaftClient::RaftClient(RaftConnection connection) : connection_(std::move(connection)) {}
bool RaftClient::doRequest(const protos::raft::StartRequest &request) {
  grpc::ClientContext client_context;
  std::unique_ptr<grpc::ClientReaderInterface<::protos::raft::StartResponse>> response =
      connection_->Start(&client_context, request);
  return false;
}
void RaftClient::start(const std::string &command) {
  protos::raft::StartRequest request = {};
  request.set_data(command);
  request.set_type(protos::raft::COMMAND);
  doRequest(request);
}
bool RaftClient::startConfig(const protos::raft::Config &config) {
  protos::raft::StartRequest request = {};
  request.set_data(config.SerializeAsString());
  request.set_type(protos::raft::CONFIG);
  doRequest(request);
}
}// namespace flashpoint::raft