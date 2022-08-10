#include "keyvalue/plugins/raft.hpp"

namespace flashpoint::keyvalue::plugins {

RaftKVPlugin::RaftKVPlugin(const std::string &peer_address) : raft_(nullptr) {
}


bool RaftKVPlugin::forward(Operation &operation) {
  raft_->start(operation.SerializeAsString());
  return true;
}

}// namespace flashpoint::keyvalue::plugins