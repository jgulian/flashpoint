#include "raft/state.hpp"
#include <unordered_map>
#include <utility>

namespace flashpoint::raft {


State::State(PeerId peer_id) : me_(std::move(peer_id)), log_(std::make_unique<std::vector<LogEntry>>()) {
  auto entry = LogEntry();
  entry.data.set_command_valid(true);
  entry.data.set_term(0);
  entry.data.set_index(0);
  entry.data.set_type(protos::raft::CMD);
  entry.data.set_data("");
  entry.fulfilled->set_value(true);
  log_->push_back(entry);
}

State::State(const PeerId &peer_id, const Config &base_config) : State(peer_id) { base_config_ = base_config; }


PeerId State::me() const { return me_; }

LogTerm State::getCurrentTerm() const { return current_term_; }

void State::setCurrentTerm(LogTerm current_term) { current_term_ = current_term; }

std::optional<PeerId> State::getVotedFor() const { return voted_for_; }

void State::setVotedFor(std::optional<PeerId> voted_for) { voted_for_ = std::move(voted_for); }

Role State::getRole() const { return role_; }

void State::setRole(Role role) { role_ = role; }

const PeerId &State::getLeaderId() const { return leader_id_; }

void State::setLeaderId(const PeerId &leader_id) { leader_id_ = leader_id; }


LogIndex State::getLogOffset() const { return log_offset_; }

void State::setLogOffset(LogIndex log_offset) { log_offset_ = log_offset; }

LogIndex State::getLogSize() const { return log_size_; }

void State::setLogSize(LogIndex log_size) { log_size_ = log_size; }

std::pair<LogIndex, LogTerm> State::getLastLogInfo() const {
  auto &log = *log_->end();
  return {log.data.index(), log.data.term()};
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

const std::vector<LogEntry> &State::getLog() const {
  return *log_;
}

bool State::cutLogToIndex(LogIndex index) {
  if (index < log_offset_)
    return false;
  if (log_size_ <= index)
    return false;

  auto delete_count = log_size_ - index;
  for (auto i = 0; i < delete_count; i++) {
    log_->back().fulfilled->set_value(false);
    log_->pop_back();
  }

  log_size_ = index;
  return true;
}

void State::appendToLog(LogEntry entry) {
  log_->emplace_back(std::move(entry));
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

void State::getRequestVoteRequest(RequestVoteRequest &request) const {
  request.set_term(current_term_);
  request.set_candidate_id(me_);
  auto [last_log_index, last_log_term] = getLastLogInfo();
  request.set_last_log_index(last_log_index);
  request.set_last_log_term(last_log_term);
}

void State::getAppendEntriesRequest(const PeerId &peer_id,
                                    AppendEntriesRequest &request) const {
  request.set_term(current_term_);
  request.set_leader_id(me_);

  auto [match_index, next_index] = getPeerIndices(peer_id);
  auto match_log = atLogIndex(match_index);
  if (!match_log.has_value())
    throw std::runtime_error("should have installed snapshot");

  request.set_prev_log_index(match_index);
  request.set_prev_log_term(match_log->get().data.term());

  //TODO: use next index to add entries

  request.set_leader_commit_index(commit_index_);
}

void State::getInstallSnapshotRequest(const PeerId &peer_id,
                                      InstallSnapshotRequest &request) const {}

int State::getPeerCount() const { return static_cast<int>(peers_.size()); }


const std::unordered_map<PeerId, State::PeerState> &State::getPeers() const { return peers_; }

std::pair<LogIndex, LogIndex> State::getPeerIndices(const PeerId &peer_id) const {
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


void State::commitConfig(LogIndex &index) {}

Config State::getBaseConfig() {
  return base_config_;
}

Peer State::findPeer(const std::string &peer_id) {
  for (int i = 0; i < base_config_.peers_size(); i++)
    if (base_config_.peers(i).id() == peer_id)
      return base_config_.peers(i);

  throw std::runtime_error("peer not found");
}


std::unique_lock<std::shared_mutex> State::acquireWriteLock() const {
  return std::move(std::unique_lock<std::shared_mutex>(lock_));
}

std::shared_lock<std::shared_mutex> State::acquireReadLock() const {
  return std::move(std::shared_lock<std::shared_mutex>(lock_));
}

}// namespace flashpoint::raft
