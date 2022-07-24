#ifndef FLASHPOINT_RAFT_RAFT_H_
#define FLASHPOINT_RAFT_RAFT_H_

#include <memory>
#include <queue>
#include <set>
#include <thread>
#include <type_traits>
#include <unordered_set>
#include <vector>

#include "containers/channel.hpp"
#include "raft/state.hpp"
#include "util/logger.hpp"
#include "util/thread_pool.hpp"
#include "util/random.hpp"

namespace flashpoint::raft {

using namespace std::chrono_literals;
constexpr auto ElectionTimeout = 2000ms;
constexpr auto MinSleepTime = 700ms;
constexpr auto MaxSleepTime = 1000ms;

class Raft {
 public:
  explicit Raft(const PeerId &peer_id,
                std::function<void(std::string)> do_command,
                std::shared_ptr<util::Logger> logger = nullptr,
                util::DefaultRandom random = {});

  ~Raft();



  void run();

  void kill();

  void forceKill();



  std::pair<LogIndex, bool> start(const std::string &data);

  bool snapshot(LogIndex last_included_index, const std::string &snapshot);

  std::pair<LogTerm, bool> getState() const;

  PeerId getId();

  PeerId getLeaderId();

 protected:

  virtual bool appendEntries(const PeerId &peer_id,
                             const AppendEntriesRequest &request,
                             AppendEntriesResponse &response) = 0;

  virtual bool installSnapshot(const PeerId &peer_id,
                               const InstallSnapshotRequest &request,
                               InstallSnapshotResponse &response) = 0;

  virtual bool requestVote(const PeerId &peer_id, const RequestVoteRequest &request,
                           RequestVoteResponse &response) = 0;



  virtual void registerPeer(const PeerId &peer_id, const std::string &peer_data) = 0;

  virtual void unregisterPeer(const PeerId &peer_id) = 0;

  std::pair<LogIndex, bool> startPeer(PeerId &peer_id, std::string data);



  void receiveAppendEntries(const AppendEntriesRequest &request,
                            AppendEntriesResponse &response);

  void receiveInstallSnapshot(const InstallSnapshotRequest &request,
                              InstallSnapshotResponse &response);

  void receiveRequestVote(const RequestVoteRequest &request,
                          RequestVoteResponse &response);

  State state_;
 private:
  void worker();

  void leaderElection(LogTerm term);

  void updateFollower(const PeerId &peer_id);

  void raiseCommitIndex();

  void commitEntries();



  util::DefaultRandom random_;

  std::atomic<bool> running_;
  std::thread thread_;
  util::ThreadPool thread_pool_ = util::ThreadPool(4);

  std::function<void(std::string)> do_command_;
  std::shared_ptr<util::Logger> logger_;
};
} // namespace flashpoint::raft

#endif // FLASHPOINT_RAFT_RAFT_H_
