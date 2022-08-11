#ifndef FLASHPOINT_RAFT_HPP
#define FLASHPOINT_RAFT_HPP

#include <grpcpp/server_builder.h>

#include "raft/grpc.hpp"
#include "raft/join_grpc.hpp"

#include "keyvalue/keyvalue.hpp"

namespace flashpoint::keyvalue::plugins {

class RaftKVPlugin : public Plugin {
 private:
  std::unique_ptr<raft::GrpcRaft> raft_;
  std::unique_ptr<grpc::Server> server_;

 public:
  explicit RaftKVPlugin(const std::string &host_address);

  explicit RaftKVPlugin(const std::string &host_address, const std::string &peer_address);


  bool forward(Operation &operation) override;

 private:
  void runCommand(raft::Command data);
};

}// namespace flashpoint::keyvalue::plugins


#endif//FLASHPOINT_RAFT_HPP
