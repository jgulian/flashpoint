#include "keyvalue/plugins/raft.hpp"

namespace flashpoint::keyvalue::plugins {

RaftKVPlugin::RaftKVPlugin(const std::string &peer_address) : raft_(nullptr) {
}


bool RaftKVPlugin::forward(Operation &operation) {
  auto entry = raft_->start(operation.SerializeAsString());
  if (!entry.has_value()) {
    operation.mutable_status()->set_code(protos::kv::Code::Error);
    operation.mutable_status()->set_info("Must run operation on leader raft");
    return false;
  } else {
    auto future = entry.value().fulfilled->get_future();
    future.wait();
    return future.get();
  }
}

}// namespace flashpoint::keyvalue::plugins