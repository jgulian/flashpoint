#include "raft/raft_tester.hpp"

namespace flashpoint::test::raft {

RaftTester::RaftTester(bool use_configs, const std::shared_ptr<util::Logger> &logger, util::DefaultRandom random)
    : raft_manager_(std::make_unique<InMemoryRaftManager>(use_configs, logger, random)), logger_(logger) {}

bool operator<(const RaftTester::RaftData &x, const RaftTester::RaftData &y) {
  return x.peer_id < y.peer_id;
}

}

