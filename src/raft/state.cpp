#include "raft/state.hpp"
#include <unordered_map>

namespace flashpoint::raft {

void State::updateTermRole(LogTerm new_term, Role new_role) {
  current_term_ = new_term;
  role_ = new_role;
  return true;
}
std::pair<LogTerm, Role> State::termRole() const {
  return {current_term_, role_};
}

void State::receivedHeartbeat() { last_heartbeat_ = now(); }
Time State::hasHeartbeatTimeoutElapsed() const {
  return last_heartbeat_;
}

void State::raiseCommitIndex(const std::unordered_map<PeerId, Peer> &peers) {
  auto peer_count = peers.size();

  std::vector<int> match_indexes(peer_count);
  for (auto &peer : peers) {
    std::lock_guard<std::mutex> peer_lock_guard(peer.lock_);
    match_indexes.emplace_back(peer.getIndices());
  }

  int median_index = static_cast<int>(peer_count) / 2 + 1;
  std::nth_element(match_indexes.begin(), match_indexes.begin() + median_index,
                   match_indexes.end());
  commit_index_ = match_indexes[peer_count / 2 + 1];
}
bool State::hasUncommittedEntry() const {
  return last_applied_ != commit_index_;
}
const protos::raft::LogEntry &State::commitEntry() { return <#initializer #>; }

LogTerm State::getTermForIndex(LogIndex index) const { return 0; }

protos::raft::RequestVoteRequest State::getVoteRequestRequest() const {}

bool State::LogSliceRange::LogSliceRangeIterator::operator==(
    const State::LogSliceRange::LogSliceRangeIterator &rhs) const {
  return index == rhs.index;
}
bool State::LogSliceRange::LogSliceRangeIterator::operator!=(
    const State::LogSliceRange::LogSliceRangeIterator &rhs) const {
  return !(rhs == *this);
}

State::LogSliceRange::LogSliceRange(LogIndex start, LogIndex end)
    : start_index(start), end_index(end) {}
State::LogSliceRange::LogSliceRangeIterator State::LogSliceRange::start() {
  return {start_index};
}
State::LogSliceRange::LogSliceRangeIterator State::LogSliceRange::end() {
  return {end_index};
}

} // namespace flashpoint::raft
