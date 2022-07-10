#include "raft/raft.hpp"

namespace flashpoint::raft {

Raft::Raft(const std::function<void(std::string)> &do_command)
    : do_command_(std::move(do_command)) {}

Raft::~Raft() {
  forceKill();
}

void Raft::kill() { running_ = false; }

void Raft::forceKill() {
  if (running_) {
    kill();
    leader_thread_.join();
    thread_.join();
  }
}

std::pair<LogIndex, bool> Raft::start(const std::string &data) {
  auto write_lock = state_.acquireWriteLock();
  if (state_.getRole() != LEADER)
    return {0, false};

  auto [last_log_index, _] = state_.getLastLogInfo();

  LogEntry entry = {};
  entry.set_index(last_log_index + 1);
  entry.set_term(state_.getCurrentTerm());
  entry.set_rsm_data(data);
  entry.set_command_valid(false);
  state_.appendToLog(entry);

  return {entry.index(), true};
}

bool Raft::snapshot(LogIndex last_included_index, const std::string &snapshot) {
  return false;
}

std::pair<LogTerm, bool> Raft::getState() const {
  auto read_lock = state_.acquireReadLock();
  return {state_.getCurrentTerm(), state_.getRole() == LEADER};
}

PeerId Raft::getId() {
  auto read_lock = state_.acquireReadLock();
  return state_.me();
}

PeerId Raft::getLeaderId() {
  auto read_lock = state_.acquireReadLock();
  return state_.getLeaderId();
}



void Raft::receiveAppendEntries(const AppendEntriesRequest &request,
                                AppendEntriesResponse &response) {
  auto lock = state_.acquireWriteLock();
  auto current_term = state_.getCurrentTerm();

  response.set_term(current_term);
  if (current_term < request.term())
    current_term = request.term();
  if (request.term() < current_term) {
    response.set_success(false);
    return;
  }

  auto [last_log_index, _] = state_.getLastLogInfo();
  if (last_log_index < request.prev_log_index()) {
    response.set_conflict_index(last_log_index + 1);
    response.set_conflict_term(-1);
    response.set_success(false);
    return;
  } else if (auto conflict_entry = state_.atLogIndex(request.prev_log_index());
      !conflict_entry.has_value() ||
          conflict_entry->get().term() != request.prev_log_term()) {
    response.set_conflict_term(conflict_entry->get().term());
    LogIndex i = request.prev_log_index();
    while (true) {
      auto entry = state_.atLogIndex(i);
      if (!entry.has_value() || entry->get().term() == request.prev_log_term())
        break;
      i--;
    }
    response.set_conflict_index(i);
    response.set_success(false);
    return;
  }

  state_.cutLogToIndex(request.prev_log_index());
  for (auto &entry : request.entries())
    state_.appendToLog(entry);

  last_log_index = std::get<0>(state_.getLastLogInfo());
  state_.setCommitIndex(
      std::min(request.leader_commit_index(), last_log_index));
  state_.receivedHeartbeat();

  response.set_success(true);
}

void Raft::receiveInstallSnapshot(const InstallSnapshotRequest &request,
                                  InstallSnapshotResponse &response) {}

void Raft::receiveRequestVote(const RequestVoteRequest &request,
                              RequestVoteResponse &response) {
  auto lock = state_.acquireWriteLock();
  auto current_term = state_.getCurrentTerm();

  response.set_term(current_term);
  if (current_term < request.term()) {
    current_term = request.term();
    state_.setVotedFor(std::nullopt);
  }

  auto voted_for = state_.getVotedFor();
  auto [last_log_index, last_log_term] = state_.getLastLogInfo();

  if (request.term() < current_term ||
      (voted_for.has_value() && voted_for.value() != request.candidate_id()) ||
      (request.last_log_term() < last_log_term) ||
      (request.last_log_term() == last_log_term &&
          request.last_log_index() < last_log_index)) {
    response.set_vote_granted(false);
    return;
  }

  auto write_lock = state_.acquireWriteLock();
  if (state_.getCurrentTerm() != current_term)

    state_.receivedHeartbeat();
  response.set_vote_granted(true);
  state_.setVotedFor(request.candidate_id());
}

std::pair<LogIndex, bool> Raft::startPeer(PeerId &peer_id, std::string data) {
  return std::pair<LogIndex, bool>();
}

void Raft::worker() {
  while (running_) {
    auto current_time = std::chrono::system_clock::now();

    {
      auto state_lock = state_.acquireReadLock();
      auto role = state_.getRole();
      auto term = state_.getCurrentTerm();

      if (role == FOLLOWER) {
        if (std::chrono::system_clock::now() - state_.getLastHeartbeat() > ElectionTimeout) {
          state_.setCurrentTerm(term + 1);
          state_.setRole(CANDIDATE);
        }
      } else if (role == CANDIDATE) {
        leaderElection();
      } else {
        state_.setCommitIndex(0);
      }

      commitEntries();

      auto sleep_time =
          random_.randomDurationBetween(MinSleepTime, MaxSleepTime);
      std::this_thread::sleep_until(current_time + sleep_time);
    }
  }
}

void Raft::leaderWorker() {
  using NextUpdate = std::pair<Time, PeerId>;
  std::priority_queue<NextUpdate, std::vector<NextUpdate>, std::greater<>>
      update_queue;
  containers::QueueChannel<NextUpdate> channel;
  std::unordered_set<PeerId> peer_set;

  while (running_) {
    auto read_lock = state_.acquireReadLock();

    if (state_.getPeerCount() != peer_set.size())
      for (auto &peer : state_.getPeers())
        if (!peer_set.contains(peer.first))
          peer_set.insert(peer.first);

    while (update_queue.top().first < std::chrono::system_clock::now()) {
      auto peer_id = update_queue.top().second;
      update_queue.pop();

      thread_pool_.newTask([this, peer_id, &channel]() {
        auto delay = updateFollower(peer_id);
        if (delay.has_value()) {
          auto delay_time = delay ? random_.randomDurationBetween(MinSleepTime, MaxSleepTime) : 0ms;
          auto next_time = std::chrono::system_clock::now() + delay_time;
          channel.write({next_time, peer_id});
        }
      });
    }

    channel.wait();
    while (!channel.empty())
      update_queue.push(channel.read());
  }
}

void Raft::leaderElection() {
  auto state_lock = state_.acquireWriteLock();

  auto term = state_.getCurrentTerm();
  state_.setVotedFor(state_.me());
  state_.receivedHeartbeat();

  auto request = RequestVoteRequest{};
  state_.getRequestVoteRequest(request);
  auto peer_count = state_.getPeerCount();
  containers::Channel<RequestVoteResponse> channel(peer_count);
  std::atomic<bool> continue_running = true;

  for (auto &peer_data : state_.getPeers()) {
    auto peer_id = peer_data.first;
    thread_pool_.newTask([&]() {
      auto read_lock = state_.acquireReadLock();

      RequestVoteResponse response;
      auto ok = requestVote(peer_id, request, response);

      if (!ok)
        response.set_vote_granted(false);

      channel.write(std::move(response));
    });
  }

  int vote_count = 1;
  for (int i = 1; i < peer_count && vote_count <= peer_count / 2; i++) {
    auto response = channel.read();

    if (response.vote_granted())
      vote_count++;

    if (term < response.term()) {
      state_.setCurrentTerm(response.term());
      state_.setRole(FOLLOWER);
      return;
    }
  }

  if (vote_count > peer_count / 2) {
    state_.setRole(LEADER);
    for (auto &peer : state_.getPeers())
      state_.setPeerIndices(peer.first, {0, 1});
    leader_thread_ = std::thread(&Raft::leaderWorker, this);
  } else {
    state_.setRole(FOLLOWER);
  }
}

std::optional<bool> Raft::updateFollower(const PeerId &peer_id) {
  return std::nullopt;
}

void Raft::raiseCommitIndex() {}

void Raft::commitEntries() {
  auto last_applied = state_.getLastApplied();
  while (last_applied < state_.getCommitIndex()) {
    last_applied++;
    auto entry = state_.atLogIndex(last_applied);
    if (!entry.has_value())
      throw std::runtime_error("no such log entry");

    if (entry->get().has_rsm_data())
      do_command_(entry->get().rsm_data());
    else {
      std::unordered_map<std::string, std::string> additions;
      std::unordered_set<std::string> removals;
      state_.configChanges(entry->get(), additions, removals);
      for (auto [peer_id, peer_data] : additions)
        registerPeer(peer_id, std::move(peer_data));
      for (auto peer_id : removals)
        unregisterPeer(peer_id);
    }
  }
}

} // namespace flashpoint::raft