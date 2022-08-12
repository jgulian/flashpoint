#include "raft/raft2.hpp"
#include <algorithm>

namespace flashpoint::raft {

const PeerId &Raft2::RaftPeer::peerId() const {
  return peer.id();
}

Raft2::Raft2(const protos::raft::Peer &me, std::function<void(protos::raft::LogEntry log_entry)> apply_command, std::optional<protos::raft::Config> starting_config) : apply_command_(std::move(apply_command)) {
  if (starting_config.has_value()) {
    base_config_.CopyFrom(starting_config.value());
  } else {
    base_config_.mutable_peers()->Add()->CopyFrom(me);
  }
  me_ = me.id();
}
Raft2::Raft2(const protos::raft::Peer &me, std::function<void(protos::raft::LogEntry log_entry)> apply_command, std::function<void(protos::raft::LogEntry log_entry)> apply_config_update, std::optional<protos::raft::Config> starting_config)
    : Raft2(me, std::move(apply_command), std::move(starting_config)) {
  apply_config_update_ = std::move(apply_config_update);
}

bool Raft2::run() {
  bool running = false;
  bool successful = running_->compare_exchange_strong(running, true);
  if (!successful && running)
    throw std::runtime_error("raft is already running");
  else if (!successful)
    return false;

  worker_function_ = std::make_unique<std::thread>(&Raft2::worker, this);
  return true;
}
bool Raft2::kill() {
  bool running = false;
  bool successful = running_->compare_exchange_strong(running, true);
  if (!successful && running)
    throw std::runtime_error("raft is not running");
  else if (!successful)
    return false;

  worker_function_->join();
  return true;
}

StartResult Raft2::start(std::string command) {
  std::lock_guard<std::mutex> lock_guard(*lock_);

  if (role_ != LEADER)
    throw std::runtime_error("can not start on non-leader");

  RaftLogEntry entry = {};
  entry.base.set_index(log_size_++);
  entry.base.set_term(current_term_);
  entry.base.set_data(std::move(command));
  entry.base.set_type(protos::raft::CMD);
  entry.base.set_command_valid(false);

  return {entry.base.index(), entry.fulfillment_status};
}
StartResult Raft2::startConfig(const protos::raft::Config &config) {
  std::lock_guard<std::mutex> lock_guard(*lock_);

  if (role_ != LEADER)
    throw std::runtime_error("can not start on non-leader");

  RaftLogEntry entry = {};
  entry.base.set_index(log_size_++);
  entry.base.set_term(current_term_);
  entry.base.set_data(config.SerializeAsString());
  entry.base.set_type(protos::raft::CONFIG);
  entry.base.set_command_valid(false);

  return {entry.base.index(), entry.fulfillment_status};
}

void Raft2::snapshot(LogIndex index, std::string snapshot) {
  std::lock_guard<std::mutex> lock_guard(*lock_);

  if (role_ != LEADER)
    throw std::runtime_error("can not start on non-leader");

  RaftLogEntry entry = {};
  entry.base.set_index(log_size_++);
  entry.base.set_term(current_term_);
  entry.base.set_type(protos::raft::CMD);
  entry.base.set_command_valid(false);

  throw std::runtime_error("not implemented yet");

  //return {entry.base.index(), entry.fulfillment_status};
}

void Raft2::worker() {
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
void Raft2::leaderElection(LogIndex expected_term) {
}
void Raft2::updateIndices() {
  while (commit_index_ < log_size_) {
    if (!hasAgreement(base_config_, commit_index_ + 1)) return;

    for (const auto &config_index : proposed_configs_) {
      const auto &config = log_
    }
  }
}
bool Raft2::hasAgreement(const protos::raft::Config &config, LogIndex index) const {
  auto agree_count = 0;
  for (const auto &peer : peers_)
    for (const auto &config_peer : config.peers())
      if (config_peer.id() == peer.peerId())
        agree_count++;
  return agree_count > (config.peers_size() / 2);
}
void Raft2::updateFollowers() {
  for (const auto &peer : peers_) {
    if (me_ == peer.peerId()) continue;
    auto peer_id = peer.peerId();
    util::THREAD_POOL->newTask([=]() { updateFollower(peer_id); });
  }
}
void Raft2::updateFollower(PeerId peer_id) {
}

const Raft2::RaftLogEntry &Raft2::atLogIndex(LogIndex index) {
  return;
}

grpc::Status Raft2::AppendEntries(::grpc::ServerContext *context, const ::protos::raft::AppendEntriesRequest *request, ::protos::raft::AppendEntriesResponse *response) {
  return grpc::Status::OK;
}
grpc::Status Raft2::RequestVote(::grpc::ServerContext *context, const ::protos::raft::RequestVoteRequest *request, ::protos::raft::RequestVoteResponse *response) {
  return grpc::Status::OK;
}
grpc::Status Raft2::InstallSnapshot(::grpc::ServerContext *context, const ::protos::raft::InstallSnapshotRequest *request, ::protos::raft::InstallSnapshotResponse *response) {
  return grpc::Status::OK;
}
}// namespace flashpoint::raft