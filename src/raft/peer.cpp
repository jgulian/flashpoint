#include "raft/peer.hpp"

namespace flashpoint::raft {

Peer::Peer(std::string client_address) {}

void Peer::getRequestVoteRequest(const State &state,
                                 RequestVoteRequest &request) {
  auto me = state.me();
  auto [term, _] = state.termRole();
  auto [last_log_index, last_log_term] = state.getLastLogInfo();

  request.set_term(term);
  request.set_candidate_id(me);
  request.set_last_log_index(last_log_index);
  request.set_last_log_term(last_log_term);
}

bool Peer::update(State &state) {
  std::lock_guard<std::mutex> peer_lock(latch_);
  AppendEntriesRequest request = {};

  {
    std::lock_guard<std::mutex> state_lk(state.lock_);
    getAppendEntriesRequest(state, request);
  }

  AppendEntriesResponse response = {};
  appendEntries(request, response);

  if (response.success()) {
    auto prev_index = request.prev_log_index();
    setIndices(prev_index, prev_index + 1);
    return true;
  } else {
    std::lock_guard<std::mutex> state_lk(state.lock_);
    auto [_, current_term] = state.termRole();

    if (current_term < request.term()) {
      state.updateTermRole(request.term(), FOLLOWER);
    } else {
      LogIndex match_index = response.conflict_index();
      if (auto conflict_term = response.conflict_index(); conflict_term != -1) {
        auto last_index_of_term = state.getLastIndexForTerm(conflict_term);
        if (last_index_of_term.has_value())
          match_index = last_index_of_term.value();
      }
      setIndices(match_index, match_index + 1);
    }

    return false;
  }
}

void Peer::getAppendEntriesRequest(const State &state,
                                   AppendEntriesRequest &request) const {
  auto me = state.me();
  auto [term, _] = state.termRole();
  auto commit_index = state.getCommitIndex();

  request.set_term(term);
  request.set_leader_id(me);

  auto prev_log_index = next_index_ - 1;
  request.set_prev_log_index(prev_log_index);
  request.set_prev_log_index(state.getTermForIndex(prev_log_index));

  fillEntries(state, request.mutable_entries());
  request.set_leader_commit_index(commit_index);
}
void Peer::getInstallSnapshotRequest(const State &state,
                                     InstallSnapshotRequest &request) {
  auto me = state.me();
  auto [term, _] = state.termRole();
  auto commit_index = state.getCommitIndex();

  // request.set_term()
}

void Peer::fillEntries(const State &state, Peer::Entries *entries) const {
  auto [last_log_index, _] = state.getLastLogInfo();
  for (auto i = next_index_; i <= last_log_index; i++)
    *entries->Add() = state.getLogEntry(i);
}

} // namespace flashpoint::raft
