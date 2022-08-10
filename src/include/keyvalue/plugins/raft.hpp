#ifndef FLASHPOINT_RAFT_HPP
#define FLASHPOINT_RAFT_HPP

#include "raft/grpc.hpp"

#include "keyvalue/keyvalue.hpp"

namespace flashpoint::keyvalue::plugins {

class RaftKV {


 private:
  raft::GrpcRaft raft_;
};

}// namespace flashpoint::keyvalue::plugins


#endif//FLASHPOINT_RAFT_HPP
