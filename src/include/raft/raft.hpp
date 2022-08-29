#ifndef FLASHPOINT_RAFT_HPP
#define FLASHPOINT_RAFT_HPP

#include <grpcpp/create_channel.h>

#include <protos/raft.grpc.pb.h>
#include <protos/raft.pb.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <future>
#include <memory>
#include <mutex>
#include <optional>
#include <utility>
#include <variant>

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

const unsigned long long SnapshotChunkSize = 64 * 1000 * 1000;

struct RaftConfig {
  protos::raft::Peer me;
  std::function<void(protos::raft::LogEntry log_entry)> apply_command = [](const protos::raft::LogEntry &) {};
  std::function<void(protos::raft::LogEntry log_entry)> apply_config_update = [](const protos::raft::LogEntry &) {};
  std::optional<protos::raft::Config> starting_config = std::nullopt;
  std::shared_ptr<util::Random> random_ = std::make_shared<util::MTRandom>();
  std::string snapshot_file;
  std::function<void(std::string)> save_state = [](const std::string &) {};
  std::unique_ptr<grpc::CompletionQueue> completion_queue_;
};

struct RaftPeer {
  template<class Request, class Response>
  struct Call {
    grpc::ClientContext client_context = {};
    Request request = {};
    Response response = {};
    std::atomic<bool> complete = false;
    grpc::Status status = {};
  };

  using AppendEntriesCall =
      std::shared_ptr<Call<protos::raft::AppendEntriesRequest, protos::raft::AppendEntriesResponse>>;
  using InstallSnapshotCall =
      std::shared_ptr<Call<protos::raft::InstallSnapshotRequest, protos::raft::InstallSnapshotResponse>>;
  using RequestVoteCall = std::shared_ptr<Call<protos::raft::RequestVoteRequest, protos::raft::RequestVoteResponse>>;

  protos::raft::Peer peer = {};
  LogIndex match_index = 0, next_index = 0;
  SnapshotId snapshot_id = 0, chunk_offset = 0;
  RaftConnection connection;
  std::list<LogIndex> active_in_configs = {};

  std::variant<std::monostate, AppendEntriesCall, InstallSnapshotCall, RequestVoteCall> last_call = {};

 public:
  RaftPeer(protos::raft::Peer peer, RaftConnection connection);
  const PeerId &peerId() const;
};

class Raft : private protos::raft::Raft::CallbackService {
 private:
  class StartResponseReactor : public grpc::ServerWriteReactor<protos::raft::StartResponse> {
   private:
    std::unique_ptr<protos::raft::StartResponse> response_ = nullptr;
    std::atomic<bool> complete_ = false;

   public:
    ~StartResponseReactor();
    void complete(std::unique_ptr<protos::raft::StartResponse> response);

   private:
    void OnWriteDone(bool b) override;
    void OnDone() override;
  };

  struct ExtendedLogEntry {
    std::shared_ptr<StartResponseReactor> response_reactor = {};
    protos::raft::LogEntry base = {};
  };

  enum RaftRole {
    LEADER,
    FOLLOWER,
    CANDIDATE,
  };

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
  bool sent_vote_requests_ = true;
  std::list<PeerId> votes_received_ = {};


 public:
  explicit Raft(std::unique_ptr<RaftConfig> config, const protos::raft::RaftState &save_state = {});

  bool run();

  bool kill();

  bool snapshot(LogIndex included_index, std::string snapshot_file);

  void persist();

 private:
  void worker();

  void updateLeaderElection();

  void updateIndices();

  void updateFollowers();

  void updateFollower(const std::unique_ptr<RaftPeer> &peer);

  const ExtendedLogEntry &atLogIndex(LogIndex index);

  void commitEntries();

  bool checkAllConfigsAgreement(const std::list<PeerId> &agreers);

  std::list<PeerId> agreersForIndex(LogIndex index);

  void fillWithChunk(protos::raft::InstallSnapshotRequest &request);

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
  RaftClient(RaftConnection connection);

  void start(const std::string &command);

  bool startConfig(const protos::raft::Config &config);
};

}// namespace flashpoint::raft

#endif//FLASHPOINT_RAFT_HPP
