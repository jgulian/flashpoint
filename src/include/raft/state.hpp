#ifndef FLASHPOINT_RAFT_STATE_H_
#define FLASHPOINT_RAFT_STATE_H_

#include <functional>
#include <list>
#include <optional>
#include <shared_mutex>
#include <unordered_set>

#include <protos/raft.pb.h>
#include <unordered_map>
#include <utility>

#include "util.hpp"

namespace flashpoint::raft {

using namespace protos::raft;

using LogIndex = unsigned long long;
using LogTerm = unsigned int;
using PeerId = std::string;

enum Role {
  LEADER,
  CANDIDATE,
  FOLLOWER,
};

using LogEntry = protos::raft::LogEntry;

template<typename T>
using OptionalRef = std::optional<std::reference_wrapper<T>>;

class State {
  class PeerState {
    friend State;

   public:
    PeerState(std::string peer_id) : peer_id_(std::move(peer_id)), match_index_(0), next_index_(1), lock_() {}

    PeerState(const PeerState &other)
        : peer_id_(other.peer_id_), match_index_(other.match_index_), next_index_(other.next_index_), lock_() {}


   private:
    std::string peer_id_;
    LogIndex match_index_, next_index_;
    mutable std::shared_mutex lock_;
  };

  class Configuration {
    friend State;
    LogIndex index_;
    bool committed_;
    std::unordered_set<PeerId> peers_;
  };

 public:
  explicit State(const PeerId &peer_id);

  PeerId me() const;

  LogTerm getCurrentTerm() const;

  void setCurrentTerm(LogTerm current_term);

  std::optional<PeerId> getVotedFor() const;

  void setVotedFor(std::optional<PeerId> voted_for);

  Role getRole() const;

  void setRole(Role role);

  const PeerId &getLeaderId() const;

  void setLeaderId(const PeerId &leader_id);



  LogIndex getLogOffset() const;

  void setLogOffset(LogIndex log_offset);

  LogIndex getLogSize() const;

  void setLogSize(LogIndex log_size);

  std::pair<LogIndex, LogTerm> getLastLogInfo() const;

  OptionalRef<LogEntry> atLogIndex(LogIndex log_index) const;

  const std::vector<LogEntry> &getLog() const;

  bool cutLogToIndex(LogIndex index);

  void appendToLog(const LogEntry &entry);


  LogIndex getCommitIndex() const;

  void setCommitIndex(LogIndex commit_index);

  LogIndex getLastApplied() const;

  void setLastApplied(LogIndex last_applied);



  const Time &getLastHeartbeat() const;

  void receivedHeartbeat();



  void getRequestVoteRequest(RequestVoteRequest &request) const;

  void getAppendEntriesRequest(const PeerId &peer_id,
                               AppendEntriesRequest &request) const;

  void getInstallSnapshotRequest(const PeerId &peer_id,
                                 InstallSnapshotRequest &request) const;



  int getPeerCount() const;

  const std::unordered_map<PeerId, PeerState> &getPeers() const;

  std::pair<LogIndex, LogIndex> getPeerIndices(const PeerId &peer_id) const;

  void setPeerIndices(const PeerId &peer_id, std::pair<LogIndex, LogIndex> indices);

  void configChanges(const LogEntry &entry,
                     std::unordered_map<std::string, std::string> &additions,
                     std::unordered_set<std::string> &removals);

  void commitConfig(LogIndex &index);



  std::unique_lock<std::shared_mutex> acquireWriteLock() const;

  std::unique_lock<std::shared_mutex> acquireReadLock() const;

 private:
  mutable std::shared_mutex lock_;

  // Persistent
  LogTerm current_term_ = 0;
  std::optional<PeerId> voted_for_ = std::nullopt;
  Role role_ = FOLLOWER;
  LogIndex log_offset_ = 0, log_size_ = 1;
  std::unique_ptr<std::vector<protos::raft::LogEntry>> log_;

  // Volatile
  LogIndex commit_index_ = 0, last_applied_ = 0;
  Time last_heartbeat_;
  PeerId me_, leader_id_;

  std::unordered_map<PeerId, PeerState> peers_ = {};
  Config config_ = {};
  std::optional<LogIndex> next_config_;
};
} // namespace flashpoint::raft

#endif // FLASHPOINT_RAFT_STATE_H_
