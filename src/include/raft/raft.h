#ifndef FLASHPOINTDB_RAFT_H_
#define FLASHPOINTDB_RAFT_H_

#include <thread>
#include <future>
#include <functional>
#include <chrono>
#include <vector>
#include <unordered_map>
#include <queue>
#include <random>
#include <iostream>
#include <utility>
#include <thread>
#include <chrono>
#include <set>
#include <variant>

#include <grpc/grpc.h>
#include <grpcpp/create_channel.h>

#include <protos/raft.grpc.pb.h>

namespace flashpoint {

class RaftPeer;

class Raft final : public protos::raft::Raft::Service {
 public:
  explicit Raft();
  ~Raft() override;

  void serve();
  void kill();

  void start(const std::string &log_id, const std::string &data);

  void registerPeer(std::string &);

 private:
  using LogIndex = unsigned long long;
  using LogTerm = unsigned int;
  using Time = std::chrono::time_point<std::chrono::system_clock>;
  using AppendEntriesResponse = std::future<std::pair<std::unique_ptr<protos::raft::AppendEntriesReply>, Time>>;
  using RequestVoteResponse = std::future<std::pair<std::unique_ptr<protos::raft::RequestVoteReply>, Time>>;

  enum Role {
    LEADER,
    CANDIDATE,
    FOLLOWER,
  };

  struct Peer {
    int id;
    LogIndex next_index, match_index;
    std::unique_ptr<protos::raft::Raft::Stub> stub;

    std::mutex peer_latch;
  };

  static constexpr auto election_timeout_ = std::chrono::milliseconds(1000);
  static constexpr auto min_sleep_time_ = std::chrono::milliseconds(100);
  static constexpr auto additional_sleep_time_ = std::chrono::milliseconds(200);

  grpc::Status AppendEntries(::grpc::ServerContext *context,
                             const ::protos::raft::AppendEntriesArgs *request,
                             ::protos::raft::AppendEntriesReply *response) override;
  grpc::Status RequestVote(::grpc::ServerContext *context,
                           const ::protos::raft::RequestVoteArgs *request,
                           ::protos::raft::RequestVoteReply *response) override;

  decltype(auto) appendEntries(int, const protos::raft::AppendEntriesArgs&);
  decltype(auto) requestVote(int, const protos::raft::RequestVoteArgs&);

  void ticker();

  LogTerm updater();
  std::pair<LogTerm, bool> beginLeaderElection();

  void updateCommitIndex();
  void commitEntry(LogIndex entry_index);

  protos::raft::LogEntry &getLogEntry(LogIndex);
  protos::raft::LogEntry &getLastLogEntry();

  std::optional<LogIndex> getLastLogIndexOfTerm(LogTerm);

  std::thread ticker_thread_;
  std::mutex latch_;

  std::atomic<bool> running_;
  std::unordered_map<std::string, std::function<void(const std::string &)>> callbacks_;
  std::unordered_map<int, Peer> peers_;

  // Persistent;
  int me_, voted_for_;
  LogTerm current_term_;
  LogIndex log_offset_;
  std::unique_ptr<std::vector<protos::raft::LogEntry>> log_;

  // Volatile
  LogIndex commit_index_, last_applied_;
  Time last_heartbeat_;
  Role role_;
};

}

#endif //FLASHPOINTDB_RAFT_H_
