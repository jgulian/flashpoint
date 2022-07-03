#ifndef FLASHPOINT_SRC_INCLUDE_RAFT_PROTOCOL_HPP_
#define FLASHPOINT_SRC_INCLUDE_RAFT_PROTOCOL_HPP_

#include "raft/state.hpp"

#include <protos/raft.pb.h>

using namespace protos::raft;

namespace flashpoint::raft {

class Protocol {
public:
  Protocol(State &state);

  virtual bool appendEntries(PeerId peer_id,
                             const AppendEntriesRequest &request,
                             AppendEntriesResponse &response) = 0;
  virtual bool installSnapshot(PeerId peer_id,
                               const InstallSnapshotRequest &request,
                               InstallSnapshotResponse &response) = 0;
  virtual bool requestVote(PeerId peer_id, const RequestVoteRequest &request,
                           RequestVoteResponse &response) = 0;

  virtual PeerId newPeer(std::string peer_data) = 0;

protected:
  bool doAppendEntries(const AppendEntriesRequest &request,
                       AppendEntriesResponse &response);
  bool doInstallSnapshot(const InstallSnapshotRequest &request,
                         InstallSnapshotResponse &response);
  bool doRequestVote(const RequestVoteRequest &request,
                     RequestVoteResponse &response);

private:
  State &state_;
};

} // namespace flashpoint::raft

#endif // FLASHPOINT_SRC_INCLUDE_RAFT_PROTOCOL_HPP_
