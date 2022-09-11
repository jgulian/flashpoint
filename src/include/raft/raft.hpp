#ifndef FLASHPOINT_SRC_INCLUDE_RAFT_RAFT_HPP_
#define FLASHPOINT_SRC_INCLUDE_RAFT_RAFT_HPP_

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

constexpr auto kElectionTimeout = 1000ms;
constexpr auto kMinSleepTime = 100ms;
constexpr auto kMaxSleepTime = 500ms;

const unsigned long long kSnapshotChunkSize = 256; //64 * 1000 * 1000;

constexpr auto kStartRequestBuffer = 100ms;
constexpr auto kStartRequestTimeout = 1000ms;

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
  std::shared_ptr<util::Random> random = std::make_shared<util::MtRandom>();
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
  [[nodiscard]] bool Active() const;
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

  bool Run();

  bool Kill();

  bool Snapshot(LogIndex included_index);

 private:
  grpc::Status Start(::grpc::ServerContext *context, const ::protos::raft::StartRequest *request,
					 ::protos::raft::StartResponse *response) override;
  grpc::Status AppendEntries(::grpc::ServerContext *context, const ::protos::raft::AppendEntriesRequest *request,
							 ::protos::raft::AppendEntriesResponse *response) override;
  grpc::Status RequestVote(::grpc::ServerContext *context, const ::protos::raft::RequestVoteRequest *request,
						   ::protos::raft::RequestVoteResponse *response) override;
  grpc::Status InstallSnapshot(::grpc::ServerContext *context, const ::protos::raft::InstallSnapshotRequest *request,
							   ::protos::raft::InstallSnapshotResponse *response) override;

  void Worker();

  void UpdateLeaderElection();

  void UpdateIndices();

  void UpdateFollowers();

  void UpdateFollower(const protos::raft::RaftState_PeerState &peer,
					  const std::unique_ptr<ExtendedRaftPeer> &extended_peer);

  [[nodiscard]] const protos::raft::LogEntry &AtLogIndex(LogIndex index) const;

  void CommitEntries();

  [[nodiscard]] bool CheckAllConfigsAgreement(const std::list<PeerId> &agreers) const;

  [[nodiscard]] std::list<PeerId> AgreersForIndex(LogIndex index) const;

  void FillWithChunk(protos::raft::InstallSnapshotRequest &request) const;

  void RegisterNewConfig(LogIndex log_index, const protos::raft::Config &config, bool base_config = false);

  void Persist();

  static bool HasAgreement(const protos::raft::Config &config, const std::list<PeerId> &agreers);
};

}// namespace flashpoint::raft

#endif//FLASHPOINT_SRC_INCLUDE_RAFT_RAFT_HPP_
