#include "keyvalue/plugins/raft.hpp"

namespace flashpoint::keyvalue::plugins {

RaftKVPlugin::RaftKVPlugin(const std::string &host_address) {
  raft_ = std::make_unique<raft::GrpcRaft>(
      host_address, [this](raft::Command cmd) { runCommand(std::move(cmd)); }, util::DefaultRandom());

  grpc::ServerBuilder server_builder;
  server_builder.AddListeningPort(host_address, grpc::InsecureServerCredentials());
  server_builder.RegisterService(raft_.get());
  server_ = server_builder.BuildAndStart();

  raft_->run();
}


RaftKVPlugin::RaftKVPlugin(const std::string &host_address, const std::string &peer_address) {
  raft::joinRaftGrpc(host_address, peer_address, [this, &host_address]() -> raft::GrpcRaft & {
    raft_ = std::make_unique<raft::GrpcRaft>(
        host_address, [this](raft::Command cmd) { runCommand(std::move(cmd)); }, util::DefaultRandom());

    grpc::ServerBuilder server_builder;
    server_builder.AddListeningPort(host_address, grpc::InsecureServerCredentials());
    server_builder.RegisterService(raft_.get());
    server_ = server_builder.BuildAndStart();
    return *raft_;
  });
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

void RaftKVPlugin::runCommand(raft::Command cmd) {
}
}// namespace flashpoint::keyvalue::plugins