#include "raft/protocol.hpp"

namespace flashpoint::raft {


Protocol::Protocol(State &state) : state_(state) {}

bool Protocol::doAppendEntries(const AppendEntriesRequest &request,
                               AppendEntriesResponse &response) {
  
  response->set_term(current_term_);
  if (current_term_ < request->term())
    current_term_ = request->term();
  if (request->term() < current_term_) {
    response->set_success(false);
    return grpc::Status::OK;
  }
  
  auto last_log_index = getLastLogEntry().log_index();
  if (last_log_index < request->prev_log_index()) {
    response->set_conflict_index(last_log_index + 1);
    response->set_conflict_term(-1);
    response->set_success(false);
    return grpc::Status::OK;
  } else if (getLogEntry(request->prev_log_index()).log_term() != request->prev_log_index()) {
    response->set_conflict_term(getLogEntry(request->prev_log_index()).log_term());
    auto conflict_index = getLastLogIndexOfTerm(request->prev_log_term());
    response->set_conflict_index(conflict_index.has_value() ? conflict_index.value() : 0);
    response->set_success(false);
    return grpc::Status::OK;
  }

  for (LogIndex i = request->prev_log_index(); i < last_log_index; i++)
    log_->pop_back();
  for (auto &entry : request->entries())
    log_->emplace_back(entry);

  commit_index_ = std::min(request->leader_commit_index(), getLastLogEntry().log_index());
  last_heartbeat_ = std::chrono::system_clock::now();

  response->set_update_index(getLastLogEntry().log_index());
  response->set_success(true);
  
  return false;
}
bool Protocol::doInstallSnapshot(const InstallSnapshotRequest &request,
                                 InstallSnapshotResponse &response) {
  
  
  auto last_log_entry = getLastLogEntry();
  response->set_term(current_term_);

  if (current_term_ < request->term()) {
    current_term_ = request->term();
    voted_for_ = -1;
  }

  if (request->term() < current_term_ ||
      (voted_for_ != -1 && voted_for_ != request->candidate_id()) ||
      (request->last_log_term() < last_log_entry.log_term()) ||
      (request->last_log_term() == last_log_entry.log_term() && request->last_log_index() < last_log_entry.log_index())) {
    response->set_vote_granted(false);
    return true;
  }

  last_heartbeat_ = std::chrono::system_clock::now();
  response->set_vote_granted(true);
  voted_for_ = request->candidate_id();
  
  return true;
}
bool Protocol::doRequestVote(const RequestVoteRequest &request,
                             RequestVoteResponse &response) {
  return false;
}

}