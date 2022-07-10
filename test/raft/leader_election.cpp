#include <memory>
#include <string>
#include <optional>
#include <thread>

#include "gtest/gtest.h"

#include "raft/raft.hpp"
#include "raft/in_memory.hpp"

namespace flashpoint::test::raft {

class RaftLeaderElection : public ::testing::Test {
 public:
  void sleepForElectionTimeoutTimes(int x) {
    std::this_thread::sleep_for(ElectionTimeout * x);
  }

  void setPeerCount(int n) {
    for (int i = 0; i < n; i++) {
      auto id = std::to_string(current_peer_id_);
      auto raft = raft_manager_->createPeer(id);
      rafts_.insert(raft);
      current_peer_id_++;
    }
  }

  std::optional<PeerId> checkForLeader() {
    std::optional<PeerId> leader = std::nullopt;
    std::cout << "HERE" << std::endl;
    for (auto &raft : rafts_) {
      std::cout << raft->getId() << std::endl;
      auto leader_id = raft->getLeaderId();
      auto [_, is_leader] = raft->getState();
      if (!leader.has_value())
        leader = leader_id;
      else if (leader.has_value() && leader.value() != leader_id ||
          leader.has_value() && is_leader && leader.value() != raft->getId())
        return std::nullopt;
    }
    return leader;
  }

  std::optional<LogTerm> agreedTerm() {
    std::optional<LogTerm> term = std::nullopt;
    for (auto &raft : rafts_) {
      auto [peer_term, _] = raft->getState();
      if (term.has_value() && term.value() != peer_term)
        return std::nullopt;
      else if (!term.has_value())
        term = peer_term;
    }
    return term;
  }

  std::unique_ptr<InMemoryRaftManager> raft_manager_;
  std::vector<std::string> log_ = {};
  std::set<std::shared_ptr<InMemoryRaftManager::InMemoryRaft>> rafts_ = {};
 protected:
  RaftLeaderElection() : raft_manager_() {
    raft_manager_ = std::make_unique<InMemoryRaftManager>(
        [this](std::string command) {
          log_.emplace_back(std::move(command));
        }
    );
  }

  ~RaftLeaderElection() override = default;

  void SetUp() override {
  }

  void TearDown() override {
    for (auto &raft : rafts_) {
      auto id = raft->getId();
      raft_manager_->destroyPeer(id);
    }
    rafts_.clear();
  }

  int current_peer_id_ = 0;
};

TEST_F(RaftLeaderElection, SimpleLeaderElection) {
  std::cout << "JERE1 " << std::endl;
  setPeerCount(3);
  std::cout << "JERE2 " << std::endl;
  sleepForElectionTimeoutTimes(2);
  std::cout << "JERE3 " << std::endl;

  auto term = agreedTerm();
  std::cout << "JERE4 " << std::endl;
  ASSERT_TRUE(term.has_value());
  ASSERT_NE(term.value(), 0);

  auto leader = checkForLeader();
  ASSERT_TRUE(leader.has_value());

  sleepForElectionTimeoutTimes(2);
  auto new_term = agreedTerm();
  ASSERT_EQ(term, new_term);
}


}
