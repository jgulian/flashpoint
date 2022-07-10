#include "raft/state.hpp"
#include <unordered_map>

namespace flashpoint::raft {

PeerId State::me() const { return me_; }

LogTerm State::getCurrentTerm() const { return current_term_; }

void State::setCurrentTerm(LogTerm current_term) {current_term_ = current_term;}

std::optional<PeerId> State::getVotedFor() const { return voted_for_; }

void State::setVotedFor(std::optional<PeerId> voted_for) { voted_for_ = std::move(voted_for); }

Role State::getRole() const { return role_; }

void State::setRole(Role role) { role_ = role; }

const PeerId &State::getLeaderId() const {return leader_id_;}

void State::setLeaderId(const PeerId &leader_id) {leader_id_ = leader_id;}



LogIndex State::getLogOffset() const { return log_offset_; }

void State::setLogOffset(LogIndex log_offset) { log_offset_ = log_offset; }

LogIndex State::getLogSize() const { return log_size_; }

void State::setLogSize(LogIndex log_size) { log_size_ = log_size; }

std::pair<LogIndex, LogTerm> State::getLastLogInfo() {
  auto &log = *log_->end();
  return {log.index(), log.term()};
}

OptionalRef<LogEntry> State::atLogIndex(LogIndex log_index) const {
  if (log_->empty())
    return std::nullopt;

  if (log_index < log_offset_)
    return std::nullopt;
  if (log_size_ <= log_index)
    return std::nullopt;

  return log_->at(log_index - log_offset_);
}

const std::vector<protos::raft::LogEntry> &State::getLog() const {
  return *log_;
}

bool State::cutLogToIndex(LogIndex index) {
  if (index < log_offset_)
    return false;
  if (log_size_ <= index)
    return false;

  log_->erase(log_->begin() + static_cast<int>(index - log_offset_), log_->end());
  log_size_ = index + 1;
  return true;
}

void State::appendToLog(const LogEntry &entry) {
  log_->emplace_back(entry);
  log_size_++;
}



LogIndex State::getCommitIndex() const { return commit_index_; }

void State::setCommitIndex(LogIndex commit_index) {
  commit_index_ = commit_index;
}

LogIndex State::getLastApplied() const { return last_applied_; }

void State::setLastApplied(LogIndex last_applied) {
  last_applied_ = last_applied;
}

const Time &State::getLastHeartbeat() const { return last_heartbeat_; }

void State::receivedHeartbeat() { last_heartbeat_ = std::chrono::system_clock::now(); }

void State::getRequestVoteRequest(RequestVoteRequest &request) const {}

void State::getAppendEntriesRequest(PeerId &peer_id,
                                    AppendEntriesRequest &request) const {}

void State::getInstallSnapshotRequest(PeerId &peer_id,
                                      InstallSnapshotRequest &request) const {}

int State::getPeerCount() const { return static_cast<int>(peers_.size()); }


const std::unordered_map<PeerId, State::PeerState> &State::getPeers() const { return peers_; }

std::pair<LogIndex, LogIndex> State::getPeerIndices(PeerId &peer_id) const {
  auto &peer = peers_.at(peer_id);
  std::shared_lock<std::shared_mutex> lk(peer.lock_);
  return {peer.match_index_, peer.next_index_};
}

void State::setPeerIndices(const PeerId &peer_id,
                           std::pair<LogIndex, LogIndex> indices) {
  auto &peer = peers_.at(peer_id);
  std::unique_lock<std::shared_mutex> lk(peer.lock_);
  peer.match_index_ = indices.first;
  peer.next_index_ = indices.second;
}

void State::configChanges(LogEntry &entry,
                          std::unordered_map<std::string, std::string> &additions,
                          std::unordered_set<std::string> &removals) {
  if (!entry.has_config())
    throw std::runtime_error("entry is not a configuration change");

  for (auto &peer : peers_)
    removals.insert(peer.first);

  for (auto &[peer_id, peer_data] : entry.config().entries()) {
    if (removals.contains(peer_id))
      removals.erase(peer_id);
    else
      additions.insert({peer_id, peer_data});
  }
}

void State::commitConfig(LogIndex &index) {}


std::unique_lock<std::shared_mutex> State::acquireWriteLock() const {
  return std::move(std::unique_lock<std::shared_mutex>(lock_));
}

std::shared_lock<std::shared_mutex> State::acquireReadLock() const {
  return std::move(std::shared_lock<std::shared_mutex>(lock_));
}

} // namespace flashpoint::raft
