#ifndef FLASHPOINT_RAFT_HPP
#define FLASHPOINT_RAFT_HPP

#include <grpcpp/create_channel.h>

#include <protos/raft.grpc.pb.h>

#include <filesystem>
#include <future>
#include <memory>
#include <mutex>
#include <optional>
#include <utility>

#include "util/logger.hpp"
#include "util/random.hpp"

#include "protos/raft.pb.h"
#include "raft/errors.hpp"

namespace flashpoint::raft {

using LogIndex = unsigned long long;
using LogTerm = unsigned long;
using SnapshotId = unsigned long;
using PeerId = std::string;
using RaftTime = std::chrono::time_point<std::chrono::system_clock>;
using RaftConnection = std::shared_ptr<protos::raft::Raft::StubInterface>;

using namespace std::chrono_literals;

constexpr auto ElectionTimeout = 1000ms;
constexpr auto MinSleepTime = 300ms;
constexpr auto MaxSleepTime = 500ms;

struct RaftConfig {
  protos::raft::Peer me;
  std::function<void(protos::raft::LogEntry log_entry)> apply_command = [](protos::raft::LogEntry _) {};
  std::function<void(protos::raft::LogEntry log_entry)> apply_config_update = [](protos::raft::LogEntry _) {};
  std::optional<protos::raft::Config> starting_config = std::nullopt;
  std::shared_ptr<util::Random> random_ = std::make_shared<util::MTRandom>();
  std::string snapshot_file;
  std::function<void(std::string)> save_state = [](std::string _) {};
};

struct RaftPeer {
  protos::raft::Peer peer = {};
  LogIndex match_index = 0, next_index = 0;
  SnapshotId snapshot_id = 0, chunk_offset = 0;
  RaftConnection connection;
  std::mutex lock = {};
  std::list<LogIndex> active_in_configs = {};

 public:
  RaftPeer(protos::raft::Peer peer, RaftConnection connection);
  const PeerId &peerId() const;
};

struct ExtendedLogEntry {
  protos::raft::LogEntry base = {};
};

enum RaftRole {
  LEADER,
  FOLLOWER,
  CANDIDATE,
};

class Raft : private protos::raft::Raft::CallbackService {
  std::unique_ptr<std::thread> worker_function_ = nullptr;
  std::unique_ptr<std::atomic<bool>> running_ = std::make_unique<std::atomic<bool>>(false);
  std::unique_ptr<std::mutex> lock_ = std::make_unique<std::mutex>();
  std::unique_ptr<RaftConfig> raft_config_ = {};

  // Persistant
  std::list<std::unique_ptr<RaftPeer>> peers_;
  //TODO: move to unique pointer of protos::raft::RaftState
  std::vector<ExtendedLogEntry> log_ = {};
  protos::raft::Config base_config_ = {};
  std::list<LogIndex> proposed_configs_ = {};
  LogIndex log_offset_ = 0, log_size_ = 0;
  LogTerm current_term_ = 0;
  std::optional<PeerId> voted_for_ = std::nullopt;
  PeerId me_ = {};
  protos::raft::Snapshot snapshot_ = {};

  // Volatile
  LogIndex commit_index_ = 0, last_applied_ = 0;
  RaftTime last_heartbeat_ = std::chrono::system_clock::now();
  RaftRole role_ = FOLLOWER;
  PeerId leader_;


 public:
  Raft(const RaftConfig &config, const protos::raft::RaftState &save_state = {});


  bool run();

  bool kill();

  bool snapshot(LogIndex included_index, std::string snapshot_file);

  void persist();

 private:
  void worker();

  void leaderElection(LogIndex expected_term);

  void updateIndices();

  void updateFollowers();

  void updateFollower(const PeerId &peer_id);

  const ExtendedLogEntry &atLogIndex(LogIndex index);

  void commitEntries();

  std::list<PeerId> agreersForIndex(LogIndex index);

  RaftPeer &raftPeerWithId(const std::string &id);

  void respectNewLeader(const std::string &leader_id, LogTerm term);

  void updateSnapshot(protos::raft::Snapshot &&snapshot);

  grpc::ServerWriteReactor<protos::raft::StartResponse> *Start(::grpc::CallbackServerContext *context,
                                                               const ::protos::raft::StartRequest *request) override;
  grpc::ServerUnaryReactor *AppendEntries(::grpc::CallbackServerContext *context,
                                          const ::protos::raft::AppendEntriesRequest *request,
                                          ::protos::raft::AppendEntriesResponse *response) override;
  grpc::ServerUnaryReactor *RequestVote(::grpc::CallbackServerContext *context,
                                        const ::protos::raft::RequestVoteRequest *request,
                                        ::protos::raft::RequestVoteResponse *response) override;
  grpc::ServerUnaryReactor *InstallSnapshot(::grpc::CallbackServerContext *context,
                                            const ::protos::raft::InstallSnapshotRequest *request,
                                            ::protos::raft::InstallSnapshotResponse *response) override;
};

class RaftClient {
 private:
  RaftConnection connection_;

  bool doRequest(const protos::raft::StartRequest &request);

 public:
  explicit RaftClient(RaftConnection connection);

  void start(const std::string &command);

  bool startConfig(const protos::raft::Config &config);
};

}// namespace flashpoint::raft

#endif//FLASHPOINT_RAFT_HPP
