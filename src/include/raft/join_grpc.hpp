#ifndef FLASHPOINT_JOIN_GRPC_HPP
#define FLASHPOINT_JOIN_GRPC_HPP

#include "grpcpp/server_builder.h"
#include <utility>

#include "raft/grpc.hpp"

namespace flashpoint::raft {

bool joinRaftGrpc(const std::string &host_address, const std::string &known_peer_data, const std::function<GrpcRaft &()> &start_raft);

void handleJoinCluster(const protos::raft::JoinClusterRequest &request, protos::raft::JoinClusterResponse &response);

}// namespace flashpoint::raft

#endif//FLASHPOINT_JOIN_GRPC_HPP
