#include <memory>
#include <string>
#include <optional>
#include <thread>
#include <format>

#include "gtest/gtest.h"

#include "raft/raft.hpp"
#include "raft/in_memory.hpp"
#include "raft/raft_tester.hpp"

#include "util/logger.hpp"

namespace flashpoint::test::raft {

class RaftLeaderElection : public ::testing::Test {
 public:
  std::shared_ptr<util::SimpleLogger> logger_ = std::make_unique<util::SimpleLogger>(util::FATAL);
  RaftTester raft_tester1_;

 protected:
  RaftLeaderElection() : raft_tester1_(false, logger_, {}) {}

  ~RaftLeaderElection() override = default;

  void SetUp() override {}

  void TearDown() override {
  }
};

TEST_F(RaftLeaderElection, SimpleLeaderElection) {
  // sigsegv seed: {1, 2, 3, 4}
  // no leader/sigsegv seed ()
  auto seed = std::seed_seq();
  auto raft_tester_ = RaftTester(false, logger_, util::DefaultRandom(seed));

  raft_tester_.setPeerCount(3);
  raft_tester_.runRafts();

  std::cout << "here2" << std::endl;

  RaftTester::sleepForElectionTimeoutTimes(4);

  std::cout << "here3" << std::endl;

  auto term = raft_tester_.agreedTerm();
  ASSERT_TRUE(term.has_value());
  ASSERT_NE(term.value(), 0);

  std::cout << "here4" << std::endl;

  auto leader = raft_tester_.checkForLeader();
  ASSERT_TRUE(leader.has_value());

  std::cout << "here5" << std::endl;

  RaftTester::sleepForElectionTimeoutTimes(2);
  auto new_term = raft_tester_.agreedTerm();
  ASSERT_EQ(term, new_term);

  std::cout << "here6" << std::endl;
}
}
