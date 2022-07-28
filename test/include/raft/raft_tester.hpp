#ifndef FLASHPOINT_TEST_INCLUDE_RAFT_RAFT_TESTER_HPP_
#define FLASHPOINT_TEST_INCLUDE_RAFT_RAFT_TESTER_HPP_

#include <queue>
#include <string>
#include <memory>
#include <unordered_map>

#include "util/logger.hpp"

#include "raft/in_memory.hpp"

namespace flashpoint::test::raft {

class RaftTester {
 public:
  explicit RaftTester(bool use_configs,
                      const std::shared_ptr<util::Logger> &logger = nullptr,
                      util::DefaultRandom = {});

  static void sleepForElectionTimeoutTimes(int x) {
    for (int i = 0; i < x; i++)
      std::this_thread::sleep_for(ElectionTimeout);
  }

  PeerId createPeer() {
    auto id = std::to_string(current_peer_id_);
    auto log = std::make_unique<std::vector<std::string>>();
    auto raft = raft_manager_->createPeer(id, [&log](std::string command) {
      log->emplace_back(std::move(command));
    });
    rafts_.emplace(std::move(RaftData({id, raft, std::make_unique<std::mutex>(), std::move(log)})));
    current_peer_id_++;
    return id;
  }

  std::list<PeerId> setPeerCount(int n) {
    std::list<PeerId> peers = {};
    for (int i = 0; i < n; i++)
      peers.emplace_back(createPeer());

    return peers;
  }

  void runRafts() {
    for (auto &raft : rafts_)
      raft.raft->run();
  }

  std::map<int, std::optional<PeerId>> getLeaders() {
    std::map<int, std::optional<PeerId>> leaders;
    auto partitions = raft_manager_->getPartitions();

    for (auto &raft : rafts_) {
      auto [_, is_leader] = raft.raft->getState();
      if (is_leader) {
        int partition = partitions.at(raft.peer_id);
        if (leaders.contains(partition))
          leaders[partition] = std::nullopt;
        else
          leaders[partition] = raft.peer_id;
      }
    }

    return leaders;
  }

  std::optional<PeerId> checkForLeader() {
    std::optional<PeerId> leader = std::nullopt;
    auto leaders = getLeaders();
    for (auto &[_, leader_id] : leaders) {
      if (leader.has_value()) {
        leader = std::nullopt;
        break;
      } else {
        leader = leader_id;
      }
    }

    return leader;
  }

  std::optional<LogTerm> agreedTerm() {
    std::optional<LogTerm> term = std::nullopt;
    for (auto &raft : rafts_) {
      auto [peer_term, _] = raft.raft->getState();
      if (term.has_value() && term.value() != peer_term)
        return std::nullopt;
      else if (!term.has_value())
        term = peer_term;
    }
    return term;
  }

  void connect(const PeerId& peer_id, int partition = 0) {
    raft_manager_->connect(peer_id, partition);
  }

  int disconnect(const PeerId& peer_id) {
    return raft_manager_->disconnect(peer_id);
  }

  int partition(std::initializer_list<PeerId> list) {
    return raft_manager_->partition(list);
  }

  int getDefaultPartition() {
    return raft_manager_->getDefaultPartition();
  }

 private:
  struct RaftData {
   public:
    std::string peer_id;
    std::shared_ptr<InMemoryRaftManager::InMemoryRaft> raft;
    std::unique_ptr<std::mutex> log_mutex;
    std::unique_ptr<std::vector<std::string>> log;
  };

  friend bool operator<(const RaftTester::RaftData &x, const RaftTester::RaftData &y);


  std::unique_ptr<InMemoryRaftManager> raft_manager_;
  std::set<RaftData> rafts_ = {};

  std::shared_ptr<util::Logger> logger_;
  int current_peer_id_ = 0;
};

}

#endif //FLASHPOINT_TEST_INCLUDE_RAFT_RAFT_TESTER_HPP_
