#ifndef FLASHPOINT_SRC_INCLUDE_RAFT_GATEWAYS_GRPC_HPP_
#define FLASHPOINT_SRC_INCLUDE_RAFT_GATEWAYS_GRPC_HPP_

#include "raft/protocol.hpp"

namespace flashpoint::raft {

class GrpcProtocol : public Protocol {
public:
  bool appendEntries(PeerId peer_id, const AppendEntriesRequest &request,
                     AppendEntriesResponse &response) override;
  bool installSnapshot(PeerId peer_id, const InstallSnapshotRequest &request,
                       InstallSnapshotResponse &response) override;
  bool requestVote(PeerId peer_id, const RequestVoteRequest &request,
                   RequestVoteResponse &response) override;
  PeerId newPeer(std::string peer_data) override;
};

}

#endif // FLASHPOINT_SRC_INCLUDE_RAFT_GATEWAYS_GRPC_HPP_
