#ifndef FLASHPOINT_RAFT_RAFT_H_
#define FLASHPOINT_RAFT_RAFT_H_

#include <memory>
#include <queue>
#include <set>
#include <thread>
#include <type_traits>
#include <unordered_set>
#include <vector>

#include "util/thread_pool.hpp"

#include "containers/channel.hpp"
#include "raft/state.hpp"

namespace flashpoint::raft {

using namespace std::chrono_literals;
constexpr auto ElectionTimeout = 1000ms;
constexpr auto MinSleepTime = 200ms;
constexpr auto MaxSleepTime = 500ms;

class Raft {
 public:
  explicit Raft(std::function<void(const std::string &)> do_command);

  ~Raft();

  void kill();

  void forceKill();



  std::pair<LogIndex, bool> start(const std::string &data);

  bool snapshot(LogIndex last_included_index, const std::string &snapshot);

 protected:
  virtual bool appendEntries(PeerId peer_id,
                             const AppendEntriesRequest &request,
                             AppendEntriesResponse &response) = 0;

  virtual bool installSnapshot(PeerId peer_id,
                               const InstallSnapshotRequest &request,
                               InstallSnapshotResponse &response) = 0;

  virtual bool requestVote(PeerId peer_id, const RequestVoteRequest &request,
                           RequestVoteResponse &response) = 0;



  virtual void registerPeer(PeerId peer_id, std::string peer_data) = 0;

  virtual void unregisterPeer(PeerId peer_id) = 0;

  std::pair<LogIndex, bool> startPeer(PeerId peer_id, std::string data);



  void receiveAppendEntries(const AppendEntriesRequest &request,
                            AppendEntriesResponse &response);

  void receiveInstallSnapshot(const InstallSnapshotRequest &request,
                              InstallSnapshotResponse &response);

  void receiveRequestVote(const RequestVoteRequest &request,
                          RequestVoteResponse &response);

 private:
  void worker();

  void leaderWorker();

  void leaderElection();


  std::optional<bool> updateFollower(const PeerId &peer_id);

  void raiseCommitIndex();

  void commitEntries();



  Random random_;

  std::atomic<bool> running_;
  std::thread thread_, leader_thread_;
  util::ThreadPool thread_pool_ = util::ThreadPool(4);

  std::function<void(const std::string &)> do_command_;
  State state_ = {};
};
} // namespace flashpoint::raft

#endif // FLASHPOINT_RAFT_RAFT_H_
