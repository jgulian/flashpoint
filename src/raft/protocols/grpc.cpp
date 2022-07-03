#include "raft/protocols/grpc.hpp"

namespace flashpoint::raft {

bool GrpcProtocol::appendEntries(PeerId peer_id,
                                 const AppendEntriesRequest &request,
                                 AppendEntriesResponse &response) {
  return false;
}
bool GrpcProtocol::installSnapshot(PeerId peer_id,
                                   const InstallSnapshotRequest &request,
                                   InstallSnapshotResponse &response) {
  return false;
}
bool GrpcProtocol::requestVote(PeerId peer_id,
                               const RequestVoteRequest &request,
                               RequestVoteResponse &response) {
  return false;
}
PeerId GrpcProtocol::newPeer(std::string peer_data) { return 0; }
}