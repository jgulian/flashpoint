#include <memory>
#include <string>
#include <optional>
#include <thread>
#include <format>

#include "gtest/gtest.h"

#include "raft/raft.hpp"
#include "raft/in_memory.hpp"
#include "util/logger.hpp"

namespace flashpoint::test::raft {

class RaftLeaderElection : public ::testing::Test {
 public:
  void sleepForElectionTimeoutTimes(int x) {
    for (int i = 0; i < x; i++) {
      std::this_thread::sleep_for(ElectionTimeout);
      logger_->log("SLEPT FOR %D\n", i);
    }
  }

  void setPeerCount(int n) {
    for (int i = 0; i < n; i++) {
      auto id = std::to_string(current_peer_id_);
      auto raft = raft_manager_->createPeer(id);
      rafts_.insert(raft);
      current_peer_id_++;
    }
  }

  void runRafts() {
    for (auto &raft : rafts_)
      raft->run();
  }

  std::optional<PeerId> checkForLeader() {
    std::optional<PeerId> leader = std::nullopt;
    for (auto &raft : rafts_) {
      auto [_, is_leader] = raft->getState();
      std::cout << raft->getId() << ", " << is_leader << std::endl;
      if (is_leader) {
        if (leader.has_value())
          return std::nullopt;
        else
          leader = raft->getId();
      }
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
  RaftLeaderElection()
      : raft_manager_(), logger_(std::make_shared<util::SimpleLogger>(util::FATAL)) {
    raft_manager_ = std::make_unique<InMemoryRaftManager>(
        [this](std::string command) {
          log_.emplace_back(std::move(command));
        },
        false,
        logger_
    );
  }

  ~RaftLeaderElection() override = default;

  void SetUp() override {}

  void TearDown() override {
    for (auto &raft : rafts_) {
      auto id = raft->getId();
      raft_manager_->destroyPeer(id);
    }
    rafts_.clear();
  }

  std::shared_ptr<util::SimpleLogger> logger_;
  int current_peer_id_ = 0;
};

TEST_F(RaftLeaderElection, SimpleLeaderElection) {
  std::cout << "here1" << std::endl;
  setPeerCount(3);
  runRafts();

  std::cout << "here2" << std::endl;

  sleepForElectionTimeoutTimes(4);

  std::cout << "here3" << std::endl;

  auto term = agreedTerm();
  ASSERT_TRUE(term.has_value());
  ASSERT_NE(term.value(), 0);

  std::cout << "here4" << std::endl;

  auto leader = checkForLeader();
  ASSERT_TRUE(leader.has_value());

  std::cout << "here5" << std::endl;

  sleepForElectionTimeoutTimes(2);
  auto new_term = agreedTerm();
  ASSERT_EQ(term, new_term);

  std::cout << "here6" << std::endl;
}
}
