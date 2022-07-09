#ifndef FLASHPOINT_RAFT_STATE_H_
#define FLASHPOINT_RAFT_STATE_H_

#include <functional>
#include <list>
#include <optional>
#include <shared_mutex>
#include <unordered_set>

#include <protos/raft.pb.h>
#include <unordered_map>

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
  PeerId me() const;

  LogTerm getCurrentTerm() const;

  void setCurrentTerm(LogTerm current_term);

  std::optional<PeerId> getVotedFor() const;

  void setVotedFor(std::optional<PeerId> voted_for);

  Role getRole() const;

  void setRole(Role role);



  LogIndex getLogOffset() const;

  void setLogOffset(LogIndex log_offset);

  LogIndex getLogSize() const;

  void setLogSize(LogIndex log_size);

  std::pair<LogIndex, LogTerm> getLastLogInfo();

  OptionalRef<LogEntry> atLogIndex(LogIndex log_index) const;

  const std::vector<LogEntry> &getLog() const;

  bool cutLogToIndex(LogIndex index);

  void appendToLog(const LogEntry& entry);


  LogIndex getCommitIndex() const;

  void setCommitIndex(LogIndex commit_index);

  LogIndex getLastApplied() const;

  void setLastApplied(LogIndex last_applied);



  const Time &getLastHeartbeat() const;

  void receivedHeartbeat();



  void getRequestVoteRequest(RequestVoteRequest &request) const;

  void getAppendEntriesRequest(PeerId &peer_id,
                               AppendEntriesRequest &request) const;

  void getInstallSnapshotRequest(PeerId &peer_id,
                                 InstallSnapshotRequest &request) const;



  int getPeerCount() const;

  const std::unordered_map<PeerId, PeerState>& getPeers() const;

  std::pair<LogIndex, LogIndex> getPeerIndices(PeerId &peer_id) const;

  void setPeerIndices(const PeerId &peer_id, std::pair<LogIndex, LogIndex> indices);

  void configChanges(LogEntry &entry,
                     std::unordered_map<std::string, std::string> &additions,
                     std::unordered_set<std::string> &removals);

  void commitConfig(LogIndex &index);



  std::unique_lock<std::shared_mutex> acquireWriteLock() const;

  std::shared_lock<std::shared_mutex> acquireReadLock() const;

 private:
  mutable std::shared_mutex lock_;

  // Persistent
  LogTerm current_term_;
  std::optional<PeerId> voted_for_;
  Role role_;
  LogIndex log_offset_, log_size_;
  std::unique_ptr<std::vector<protos::raft::LogEntry>> log_;

  // Volatile
  LogIndex commit_index_, last_applied_;
  Time last_heartbeat_;
  PeerId me_;

  std::unordered_map<PeerId, PeerState> peers_ = {};
  Config config_ = {};
  std::optional<LogIndex> next_config_;
};
} // namespace flashpoint::raft

#endif // FLASHPOINT_RAFT_STATE_H_
