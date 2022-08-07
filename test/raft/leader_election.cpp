#include <memory>
#include <string>
#include <optional>
#include <thread>

#include "gtest/gtest.h"

#include "raft/raft.hpp"
#include "raft/in_memory.hpp"
#include "raft/raft_tester.hpp"

#include "util/logger.hpp"

namespace flashpoint::test::raft {

class RaftLeaderElection : public ::testing::Test {
 public:
  std::shared_ptr<util::SimpleLogger> logger_ = std::make_unique<util::SimpleLogger>(util::LogLevel::WARN);
  RaftTester raft_tester_;

 protected:
  RaftLeaderElection() : raft_tester_(false, logger_, {}) {}

  ~RaftLeaderElection() override = default;

  void SetUp() override {}

  void TearDown() override {
  }
};

TEST_F(RaftLeaderElection, SimpleLeaderElection) {
  // sigsegv seed: {1, 2, 3, 4}
  // no leader/sigsegv seed ()

  raft_tester_.setPeerCount(3);
  raft_tester_.runRafts();

  RaftTester::sleepForElectionTimeoutTimes(4);

  auto term = raft_tester_.agreedTerm();
  ASSERT_TRUE(term.has_value());
  ASSERT_NE(term.value(), 0);

  auto leader = raft_tester_.checkForLeader();
  ASSERT_TRUE(leader.has_value());

  RaftTester::sleepForElectionTimeoutTimes(2);
  auto new_term = raft_tester_.agreedTerm();
  ASSERT_EQ(term, new_term);
}

TEST_F(RaftLeaderElection, QuorumRequirements) {
  raft_tester_.setPeerCount(3);
  raft_tester_.runRafts();

  RaftTester::sleepForElectionTimeoutTimes(4);

  auto leader = raft_tester_.checkForLeader();
  ASSERT_TRUE(leader.has_value());

  auto first_leader_id = leader.value();
  auto new_partition = raft_tester_.disconnect(first_leader_id);

  RaftTester::sleepForElectionTimeoutTimes(4);

  leader = raft_tester_.checkForLeader();
  ASSERT_TRUE(leader.has_value());

  auto default_partition = raft_tester_.getDefaultPartition();
  auto leaders = raft_tester_.getLeaders();

  ASSERT_TRUE(leaders.contains(default_partition));
  ASSERT_TRUE(leaders.at(default_partition).has_value());
  ASSERT_STREQ(leaders.at(default_partition)->c_str(), first_leader_id.c_str());

  ASSERT_TRUE(leaders.contains(new_partition));
  ASSERT_TRUE(leaders.at(new_partition).has_value());
  auto second_leader_id = leaders.at(new_partition).value();
  ASSERT_STRNE(second_leader_id.c_str(), first_leader_id.c_str());

  raft_tester_.disconnect(second_leader_id);
}

TEST_F(RaftLeaderElection, BasicAgreement) {
  auto rafts = raft_tester_.setPeerCount(3);
  raft_tester_.runRafts();

  for (auto i = 1; i <= 3; i++) {
    ASSERT_EQ(0, raft_tester_.numCommitted(i));


  }
}
