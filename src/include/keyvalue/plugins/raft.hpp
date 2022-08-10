#ifndef FLASHPOINT_RAFT_HPP
#define FLASHPOINT_RAFT_HPP

#include "raft/grpc.hpp"

#include "keyvalue/keyvalue.hpp"

namespace flashpoint::keyvalue::plugins {

class RaftKVPlugin : public Plugin {
 public:
  explicit RaftKVPlugin(const std::string &peer_address);


  bool forward(Operation &operation) override;

 private:
  std::unique_ptr<raft::GrpcRaft> raft_;
};

}// namespace flashpoint::keyvalue::plugins


#endif//FLASHPOINT_RAFT_HPP
