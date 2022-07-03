#ifndef FLASHPOINT_RAFT_RAFT_H_
#define FLASHPOINT_RAFT_RAFT_H_

#include <queue>
#include <thread>
#include <type_traits>
#include <unordered_set>
#include <vector>

#include "util/thread_pool.hpp"

#include "containers/channel.hpp"
#include "raft/protocol.hpp"
#include "raft/state.hpp"

namespace flashpoint::raft {

using namespace std::chrono_literals;
constexpr auto ElectionTimeout = 1000ms;
constexpr auto MinSleepTime = 200ms;
constexpr auto MaxSleepTime = 500ms;

class Clerk;

template <typename P, std::enable_if_t<std::is_base_of_v<Protocol, P>>> class Raft {
public:
  void kill();

private:
  void worker();
  void leaderWorker();
  void leaderElection();

  void commitEntries();

  Random random_;

  std::atomic<bool> running_;
  std::thread thread_, leader_thread_;
  util::ThreadPool thread_pool_ = util::ThreadPool(4);

  State state_;
  Service service_;
  std::unordered_map<PeerId, Peer> peers_;
  std::list<std::reference_wrapper<Clerk>> clerks_;
};

} // namespace flashpoint::raft

#endif // FLASHPOINT_RAFT_RAFT_H_
