#ifndef FLASHPOINT_RAFT_STATE_H_
#define FLASHPOINT_RAFT_STATE_H_

#include <list>
#include <mutex>

#include <protos/raft.pb.h>
#include <unordered_map>

#include "util.hpp"

namespace flashpoint::raft {

using LogIndex = unsigned long long;
using LogTerm = unsigned int;
using PeerId = int;

enum Role {
  LEADER,
  CANDIDATE,
  FOLLOWER,
};

class Peer;

using LogEntry = protos::raft::LogEntry;

class State {
public:
  struct Peer {
    LogIndex match_index, next_index;
    mutable std::mutex lock;
  };

  mutable std::mutex lock_;

  void updateTermRole(LogTerm new_term, Role new_role);
  std::pair<LogTerm, Role> termRole() const;

  void receivedHeartbeat();
  Time hasHeartbeatTimeoutElapsed() const;

  const LogEntry &getLogEntry(LogIndex index) const;
  LogTerm getTermForIndex(LogIndex index) const;
  std::optional<LogIndex> getLastIndexForTerm(LogIndex) const;
  std::pair<LogIndex, LogTerm> getLastLogInfo() const;

  LogIndex getCommitIndex() const;
  void raiseCommitIndex(const std::unordered_map<PeerId, Peer> &peers);
  bool hasUncommittedEntry() const;
  const protos::raft::LogEntry &commitEntry();

  PeerId me() const;
  void voteFor(PeerId peer_id);

  LogIndex firstIndex() const;

  struct LogSliceRange {
    struct LogSliceRangeIterator {

      bool operator==(const LogSliceRangeIterator &rhs) const;
      bool operator!=(const LogSliceRangeIterator &rhs) const;

      const LogEntry& operator*() const;
      void operator++();

      LogIndex index;
    };

    LogSliceRange(LogIndex start, LogIndex end);
    LogSliceRangeIterator start();
    LogSliceRangeIterator end();

    LogIndex start_index, end_index;
  };

private:
  // Persistent
  LogTerm current_term_;
  int voted_for_;

  LogIndex log_offset_, log_size_;
  std::unique_ptr<std::vector<protos::raft::LogEntry>> log_;

  // Volatile
  LogIndex commit_index_, last_applied_;
  Time last_heartbeat_;
  Role role_;
  PeerId me_;
};

} // namespace flashpoint::raft

#endif // FLASHPOINT_RAFT_STATE_H_
