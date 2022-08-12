#ifndef FLASHPOINT_RAFT2_HPP
#define FLASHPOINT_RAFT2_HPP

#include <protos/raft.grpc.pb.h>

#include <future>
#include <memory>
#include <mutex>
#include <optional>
#include <utility>

#include "util/logger.hpp"
#include "util/thread_pool.hpp"

#include "raft/errors.hpp"

namespace flashpoint::raft {

using LogIndex = unsigned long long;
using LogTerm = unsigned long;
using PeerId = std::string;
using RaftTime = std::chrono::time_point<std::chrono::system_clock>;

using namespace std::chrono_literals;

constexpr auto ElectionTimeout = 1000ms;
constexpr auto MinSleepTime = 300ms;
constexpr auto MaxSleepTime = 500ms;

struct StartResult {
  LogIndex index;
  std::shared_ptr<std::promise<bool>> fulfillment_status;
};

class Raft2 : private protos::raft::Raft::Service {
  struct RaftPeer {
    protos::raft::Peer peer = {};
    LogIndex match_index = 0, next_index = 0;
    std::shared_ptr<protos::raft::Raft::StubInterface> stub;

    const PeerId &peerId() const;
  };

  struct RaftLogEntry {
    protos::raft::LogEntry base = {};
    std::shared_ptr<std::promise<bool>> fulfillment_status = std::make_shared<std::promise<bool>>();
  };

  enum RaftRole {
    LEADER,
    FOLLOWER,
    CANDIDATE,
  };

 private:
  std::unique_ptr<std::thread> worker_function_ = nullptr;//TODO: use custom scheduling
  std::unique_ptr<std::atomic<bool>> running_ = std::make_unique<std::atomic<bool>>(false);
  std::unique_ptr<std::mutex> lock_ = std::make_unique<std::mutex>();

  std::function<void(protos::raft::LogEntry log_entry)> apply_command_, apply_config_update_;

  // State
  std::list<RaftPeer> peers_;

  // Persistant
  std::list<RaftLogEntry> log_ = {};
  protos::raft::Config base_config_ = {};
  std::list<LogIndex> proposed_configs_ = {};
  LogIndex log_offset_ = 0, log_size_ = 0;
  std::optional<PeerId> voted_for_ = std::nullopt;
  LogTerm current_term_ = 0;
  PeerId me_ = {};

  // Volatile
  LogIndex commit_index_ = 0, last_applied_ = 0;
  RaftTime last_heartbeat_ = std::chrono::system_clock::now();
  RaftRole role_ = FOLLOWER;


 public:
  Raft2(const protos::raft::Peer &me, std::function<void(protos::raft::LogEntry log_entry)> apply_command, std::optional<protos::raft::Config> starting_config = std::nullopt);

  Raft2(const protos::raft::Peer &me, std::function<void(protos::raft::LogEntry log_entry)> apply_command, std::function<void(protos::raft::LogEntry log_entry)> apply_config_update, std::optional<Config> starting_config = std::nullopt);


  bool run();

  bool kill();


  StartResult start(std::string command);

  StartResult startConfig(const protos::raft::Config &config);


  void snapshot(LogIndex index, std::string snapshot);

 private:
  void worker();

  void leaderElection(LogIndex expected_term);

  void updateIndices();

  bool hasAgreement(const protos::raft::Config &config, LogIndex index) const;

  void updateFollowers();

  void updateFollower(PeerId peer_id);

  const RaftLogEntry &atLogIndex(LogIndex index);

  grpc::Status AppendEntries(::grpc::ServerContext *context, const ::protos::raft::AppendEntriesRequest *request, ::protos::raft::AppendEntriesResponse *response) override;
  grpc::Status RequestVote(::grpc::ServerContext *context, const ::protos::raft::RequestVoteRequest *request, ::protos::raft::RequestVoteResponse *response) override;
  grpc::Status InstallSnapshot(::grpc::ServerContext *context, const ::protos::raft::InstallSnapshotRequest *request, ::protos::raft::InstallSnapshotResponse *response) override;
};

}// namespace flashpoint::raft

#endif//FLASHPOINT_RAFT2_HPP
