#ifndef FLASHPOINT_TEST_INCLUDE_RAFT_IN_MEMORY_H_
#define FLASHPOINT_TEST_INCLUDE_RAFT_IN_MEMORY_H_

#include "raft/raft.hpp"
#include <protos/raft.pb.h>


using namespace flashpoint::raft;

namespace flashpoint::test::raft {

class InMemoryRaftManager {
public:
  PeerId createPeer();
  void destroyPeer(PeerId peer_id);
private:
  class InMemoryRaft : public Raft {
    ~InMemoryRaft();

  protected:
    bool appendEntries(PeerId peer_id,
                       const AppendEntriesRequest &request,
                       AppendEntriesResponse &response) override;
    bool
    installSnapshot(PeerId peer_id,
                    const InstallSnapshotRequest &request,
                    InstallSnapshotResponse &response) override;
    bool requestVote(PeerId peer_id,
                     const RequestVoteRequest &request,
                     RequestVoteResponse &response) override;
    
    PeerId registerPeer(std::string peer_data) override;
    void unregisterPeer(PeerId peer_id) override;

  private:
    PeerId id_;
  };
};

} // namespace flashpoint::test::raft

#endif // FLASHPOINT_TEST_INCLUDE_RAFT_IN_MEMORY_H_
