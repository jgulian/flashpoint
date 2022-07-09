#include "raft/in_memory.h"

namespace flashpoint::test::raft {

bool InMemoryRaftManager::InMemoryRaft::appendEntries(
    flashpoint::raft::PeerId peer_id,
    const protos::raft::AppendEntriesRequest &request,
    protos::raft::AppendEntriesResponse &response) {
  return false;
}
bool InMemoryRaftManager::InMemoryRaft::installSnapshot(
    flashpoint::raft::PeerId peer_id,
    const protos::raft::InstallSnapshotRequest &request,
    protos::raft::InstallSnapshotResponse &response) {
  return false;
}
bool InMemoryRaftManager::InMemoryRaft::requestVote(
    flashpoint::raft::PeerId peer_id,
    const protos::raft::RequestVoteRequest &request,
    protos::raft::RequestVoteResponse &response) {
  return false;
}
flashpoint::raft::PeerId
InMemoryRaftManager::InMemoryRaft::registerPeer(std::string peer_data) {
  return 0;
}
void InMemoryRaftManager::InMemoryRaft::unregisterPeer(
    flashpoint::raft::PeerId peer_id) {}
}