#ifndef FLASHPOINT_RAFT_HPP
#define FLASHPOINT_RAFT_HPP

#include <grpcpp/create_channel.h>

#include <protos/raft.grpc.pb.h>
#include <protos/raft.pb.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <future>
#include <iterator>
#include <memory>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <utility>
#include <variant>

#include "util/concurrent_queue.hpp"
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
constexpr auto MinSleepTime = 100ms;
constexpr auto MaxSleepTime = 500ms;

const unsigned long long SnapshotChunkSize = 256; //64 * 1000 * 1000;

constexpr auto StartRequestBuffer = 100ms;
constexpr auto StartRequestTimeout = 1000ms;

struct PersistenceSettings {
  PersistenceSettings() = default;
  PersistenceSettings(PersistenceSettings &&other) noexcept = default;

  std::string snapshot_file;
  std::string persistent_file;
  util::ConcurrentQueue<unsigned long long> recent_persists = {};
  unsigned long long persistence_threshold = 0;
};

struct RaftSettings {
  protos::raft::Peer me;
  std::function<void(protos::raft::LogEntry log_entry)> apply_command = [](const protos::raft::LogEntry &) {};
  std::function<void(protos::raft::LogEntry log_entry)> apply_config_update = [](const protos::raft::LogEntry &) {};
  protos::raft::Config starting_config = {};
  std::shared_ptr<util::Random> random = std::make_shared<util::MTRandom>();
  std::optional<PersistenceSettings> persistence_settings = std::nullopt;
};

struct ExtendedRaftPeer {
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

  RaftConnection connection = nullptr;
  bool active_in_base_config = false;
  std::list<LogIndex> active_in_configs = {};

  std::variant<std::monostate, AppendEntriesCall, InstallSnapshotCall, RequestVoteCall> last_call = {};

 public:
  explicit ExtendedRaftPeer(std::shared_ptr<protos::raft::Raft::StubInterface> connection);
  [[nodiscard]] bool active() const;
};

class Raft : public protos::raft::Raft::Service {
 private:
  enum RaftRole {
	LEADER,
	FOLLOWER,
	CANDIDATE,
  };

  std::unique_ptr<std::thread> worker_function_ = nullptr;
  std::unique_ptr<std::atomic<bool>> running_ = std::make_unique<std::atomic<bool>>(false);
  std::unique_ptr<std::mutex> lock_ = std::make_unique<std::mutex>();

  std::shared_ptr<RaftSettings> settings_ = {};
  std::unique_ptr<protos::raft::RaftState> raft_state_ = std::make_unique<protos::raft::RaftState>();
  std::unique_ptr<std::map<PeerId, std::unique_ptr<ExtendedRaftPeer>>> extended_peers_ =
	  std::make_unique<std::map<PeerId, std::unique_ptr<ExtendedRaftPeer>>>();

  // Volatile State
  LogIndex commit_index_ = 0, last_applied_ = 0;
  RaftTime last_heartbeat_ = std::chrono::system_clock::now();
  RaftRole role_ = FOLLOWER;
  PeerId leader_;
  bool sent_vote_requests_ = true;
  std::list<PeerId> votes_received_ = {};
  std::list<LogIndex> proposed_configs_ = {};


 public:
  explicit Raft(std::shared_ptr<RaftSettings> config, bool use_persisted_state = false);

  bool run();

  bool kill();

  bool snapshot(LogIndex included_index);

 private:
  void worker();

  void updateLeaderElection();

  void updateIndices();

  void updateFollowers();

  void updateFollower(const protos::raft::RaftState_PeerState &peer,
                      const std::unique_ptr<ExtendedRaftPeer> &extended_peer);

  [[nodiscard]] const protos::raft::LogEntry &atLogIndex(LogIndex index) const;

  void commitEntries();

  [[nodiscard]] bool checkAllConfigsAgreement(const std::list<PeerId> &agreers) const;

  [[nodiscard]] std::list<PeerId> agreersForIndex(LogIndex index) const;

  void fillWithChunk(protos::raft::InstallSnapshotRequest &request);

  void registerNewConfig(LogIndex log_index, const protos::raft::Config &config, bool base_config = false);

  grpc::Status Start(::grpc::ServerContext *context, const ::protos::raft::StartRequest *request,
                     ::protos::raft::StartResponse *response) override;
  grpc::Status AppendEntries(::grpc::ServerContext *context, const ::protos::raft::AppendEntriesRequest *request,
                             ::protos::raft::AppendEntriesResponse *response) override;
  grpc::Status RequestVote(::grpc::ServerContext *context, const ::protos::raft::RequestVoteRequest *request,
                           ::protos::raft::RequestVoteResponse *response) override;
  grpc::Status InstallSnapshot(::grpc::ServerContext *context, const ::protos::raft::InstallSnapshotRequest *request,
                               ::protos::raft::InstallSnapshotResponse *response) override;

  void persist();
};

class RaftClient {
 private:
  PeerId me_;
  protos::raft::Config config_ = {};
  std::optional<std::pair<std::string, RaftConnection>> cached_connection_info_ = std::nullopt;
  std::unique_ptr<std::shared_mutex> lock_ = std::make_unique<std::shared_mutex>();

 public:
  explicit RaftClient(PeerId me, const protos::raft::Config &config);

  void updateConfig(const protos::raft::Config &config);

  LogIndex start(const std::string &command);

  LogIndex startConfig(const protos::raft::Config &config);

 private:
  LogIndex doRequest(const protos::raft::StartRequest &request);

  void checkForCacheExistence(std::shared_lock<std::shared_mutex> &lock);

  void updateCache(std::shared_lock<std::shared_mutex> &lock, const std::string &leader_id);
};

}// namespace flashpoint::raft

#endif//FLASHPOINT_RAFT_HPP
