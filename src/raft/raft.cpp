#include "raft/raft.hpp"

#include <utility>

namespace flashpoint::raft {

Raft::Raft(const PeerId &peer_id,
           std::function<void(Command)> do_command,
           util::DefaultRandom random)
    : state_(peer_id), do_command_(std::move(do_command)), random_(random) {}

Raft::~Raft() {
  forceKill();
}



void Raft::run() {
  if (!running_) {
    running_ = true;
    thread_ = std::thread(&Raft::worker, this);
  }
}

void Raft::kill() { running_ = false; }

void Raft::forceKill() {
  if (running_) {
    kill();
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
  if (state_.getCurrentTerm() < request.term()) {
    current_term = request.term();
    state_.setCurrentTerm(request.term());
    state_.setRole(FOLLOWER);
  }

  if (request.term() < current_term) {
    response.set_success(false);
    util::LOGGER->log("%s, %d: failed append entries from %s",
                      state_.me().c_str(),
                      state_.getCurrentTerm(),
                      request.leader_id().c_str());
    return;
  }

  auto [last_log_index, _] = state_.getLastLogInfo();
  if (last_log_index < request.prev_log_index()) {
    response.set_conflict_index(last_log_index + 1);
    response.set_conflict_term(-1);
    response.set_success(false);
    util::LOGGER->log("%s: failed append entries from %s", state_.me().c_str(), request.leader_id().c_str());
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

    util::LOGGER->log("%s, %d: failed append entries from %s",
                      state_.me().c_str(),
                      state_.getCurrentTerm(),
                      request.leader_id().c_str());
    return;
  }

  state_.cutLogToIndex(request.prev_log_index() + 1);
  for (auto &entry : request.entries())
    state_.appendToLog(entry);

  util::LOGGER->log("%s: successful append entries from %s", state_.me().c_str(), request.leader_id().c_str());

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
    state_.setCurrentTerm(request.term());
    state_.setVotedFor(std::nullopt);
  }

  if (10 < current_term)
    throw std::runtime_error("this is bad");

  auto voted_for = state_.getVotedFor();
  auto [last_log_index, last_log_term] = state_.getLastLogInfo();

  if (request.term() < current_term ||
      (voted_for.has_value() && voted_for.value() != request.candidate_id()) ||
      (request.last_log_term() < last_log_term) ||
      (request.last_log_term() == last_log_term &&
          request.last_log_index() < last_log_index)) {
    response.set_vote_granted(false);
    util::LOGGER->log("%s: denied vote request from %s", state_.me().c_str(), request.candidate_id().c_str());
    return;
  }

  state_.receivedHeartbeat();
  response.set_vote_granted(true);
  state_.setVotedFor(request.candidate_id());
  util::LOGGER->log("%s: granted vote request from %s", state_.me().c_str(), request.candidate_id().c_str());
}



std::pair<LogIndex, bool> Raft::startPeer(PeerId &peer_id, std::string data) {
  return std::pair<LogIndex, bool>();
}



void Raft::worker() {
  while (running_) {
    auto current_time = std::chrono::system_clock::now();

    {
      auto state_lock = state_.acquireWriteLock();

      auto role = state_.getRole();
      auto term = state_.getCurrentTerm();

      if (role == FOLLOWER) {
        util::LOGGER->log("%s, %d: I am a follower", state_.me().c_str(), state_.getCurrentTerm());
        if (std::chrono::system_clock::now() - state_.getLastHeartbeat() > ElectionTimeout) {
          util::LOGGER->log("%s: I wish to become the leader", state_.me().c_str());
          auto new_term = term + 1;
          state_.setCurrentTerm(new_term);
          state_.setRole(CANDIDATE);
          thread_pool_.newTask([this, new_term]() -> void { leaderElection(new_term); }); // TODO: fix
        }
      } else if (role == LEADER) {
        util::LOGGER->log("%s, %d: I am the leader", state_.me().c_str(), state_.getCurrentTerm());
        state_.receivedHeartbeat();
        raiseCommitIndex();
        for (const auto &peer : state_.getPeers()) {
          if (state_.me() == peer.first) continue;
          auto peer_id = peer.first;
          thread_pool_.newTask([this, peer_id]() { updateFollower(peer_id); });
        }
      }
      commitEntries();
    }

    auto sleep_for = random_.generateDurationBetween(MinSleepTime, MaxSleepTime);
    if (std::chrono::system_clock::now() < current_time + sleep_for)
      std::this_thread::sleep_until(current_time + sleep_for);
    else
      util::LOGGER->msg(util::LogLevel::WARN, "raft worker sleep cycle missed");
  }
}

void Raft::leaderElection(LogTerm term) {
  int peer_count = -1;
  containers::QueueChannel<RequestVoteResponse> channel = {};
  RequestVoteRequest request;
  std::vector<std::future<void>> futures = {};

  {
    auto state_lock = state_.acquireReadLock();

    if (term != state_.getCurrentTerm())
      return;

    peer_count = state_.getPeerCount();
    state_.setVotedFor(state_.me());
    state_.receivedHeartbeat();
    state_.getRequestVoteRequest(request);

    for (auto &peer_data : state_.getPeers()) {
      auto peer_id = peer_data.first;
      if (peer_id == state_.me()) continue;

      auto future = thread_pool_.newTask([this, peer_id, &request, &channel]() {
        RequestVoteResponse response;
        auto ok = requestVote(peer_id, request, response);

        if (!ok)
          response.set_vote_granted(false);

        channel.write(std::move(response));
      });
      futures.emplace_back(std::move(future));
    }
  }

  int vote_count = 1;
  for (int i = 1; i < peer_count && vote_count <= peer_count / 2; i++) {
    auto response = channel.read();

    if (response.vote_granted())
      vote_count++;

    if (term < response.term()) {
      auto state_lock = state_.acquireWriteLock();
      state_.setCurrentTerm(response.term());
      state_.setRole(FOLLOWER);

      for (auto &future : futures)
        future.wait();
      return;
    }
  }

  {
    auto state_lock = state_.acquireWriteLock();

    if (term == state_.getCurrentTerm() && vote_count > peer_count / 2)
      state_.setRole(LEADER);
    else
      state_.setRole(FOLLOWER);
  }

  for (auto &future : futures)
    future.wait();
}

void Raft::updateFollower(const PeerId &peer_id) {

  util::LOGGER->log("%s: appending entries to follower: %s", state_.me().c_str(), peer_id.c_str());

  AppendEntriesRequest request;
  {
    auto read_lock = state_.acquireReadLock();
    state_.getAppendEntriesRequest(peer_id, request);
  }

  AppendEntriesResponse response = {};
  auto ok = appendEntries(peer_id, request, response);
  if (!ok) return;

  // TODO: finish this
}

void Raft::raiseCommitIndex() {
  state_.setCommitIndex(0);
}

void Raft::commitEntries() {
  auto last_applied = state_.getLastApplied();
  while (last_applied < state_.getCommitIndex()) {
    last_applied++;
    auto entry = state_.atLogIndex(last_applied);
    if (!entry.has_value())
      throw std::runtime_error("no such log entry");

    if (entry->get().has_rsm_data())
      do_command_({
                      entry->get().index(),
                      entry->get().rsm_data(),
                  });
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