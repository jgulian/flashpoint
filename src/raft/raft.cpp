#include "raft/raft.hpp"
#include "raft/clerk.hpp"
#include "raft/peer.hpp"
#include "raft/state.hpp"

namespace flashpoint::raft {

void Raft::worker() {
  while (running_) {
    auto current_time = std::chrono::system_clock::now();

    {
      std::lock_guard<std::mutex> lock_guard(state_.lock_);
      auto [role, term] = state_.termRole();

      if (role == FOLLOWER) {
        if (state_.hasHeartbeatTimeoutElapsed())
          state_.updateTermRole(term + 1, CANDIDATE);
      } else if (role == CANDIDATE) {
        leaderElection();
      } else {
        state_.raiseCommitIndex(peers_);
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
    std::lock_guard<std::mutex> lock_guard(state_.lock_);

    if (peers_.size() != peer_set.size())
      for (auto &peer : peers_)
        if (!peer_set.contains(peer.first))
          peer_set.insert(peer.first);

    while (update_queue.top().first < now()) {
      auto peer_id = update_queue.top().second;
      update_queue.pop();

      thread_pool_.newTask([this, peer_id, &channel]() {
        auto next_time = updateFollower(peer_id);
        if (next_time.has_value())
          channel.write({next_time.value(), peer_id});
      });
    }

    channel.wait();
    while (!channel.empty())
      update_queue.push(channel.read());
  }
}
void Raft::leaderElection() {
  using Response = RequestVoteResponse;

  std::lock_guard<std::mutex> lock_guard(state_.lock_);

  auto [current_term, role] = state_.termRole();
  current_term++;
  state_.updateTermRole(current_term + 1, CANDIDATE);
  state_.voteFor(state_.me());
  state_.receivedHeartbeat();

  auto request = protos::raft::RequestVoteRequest{};
  Peer::getRequestVoteRequest(state_, request);
  auto peer_count = static_cast<int>(peers_.size());
  containers::Channel<Response> channel(peer_count);
  std::atomic<bool> continue_running = true;

  for (auto &peer : peers_) {
    thread_pool_.newTask([&]() {
      std::lock_guard<std::mutex> lock(peer.second.lock_);

      Response response;
      auto ok = peer.second.requestVote(request, response);

      while (!ok && continue_running)
        ok = peer.second.requestVote(request, response);
      if (ok)
        channel.write(std::move(response));
    });
  }

  int vote_count = 1;
  for (int i = 1; i < peer_count && vote_count <= peer_count / 2; i++) {
    auto response = channel.read();

    if (response.vote_granted())
      vote_count++;

    if (current_term < response.term()) {
      state_.updateTermRole(response.term(), FOLLOWER);
      return;
    }
  }

  if (vote_count > peer_count / 2) {
    state_.updateTermRole(current_term, LEADER);
    for (auto &peer : peers_)
      peer.second.setIndices(0, 1);
    leader_thread_ = std::thread(&Raft::leaderWorker, this);
  } else {
    state_.updateTermRole(current_term, FOLLOWER);
  }
}

void Raft::commitEntries() {
  while (state_.hasUncommittedEntry()) {
    auto entry = state_.commitEntry();
    for (auto &clerk : clerks_)
      if (clerk.get().getClerkId() == entry.log_id())
        clerk.get().commit(entry.log_id());
  }
}

} // namespace flashpoint::raft