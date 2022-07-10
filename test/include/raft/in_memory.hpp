#ifndef FLASHPOINT_TEST_INCLUDE_RAFT_IN_MEMORY_HPP_
#define FLASHPOINT_TEST_INCLUDE_RAFT_IN_MEMORY_HPP_

#include <stdarg.h>

#include <utility>
#include <unordered_map>
#include <memory>
#include <set>
#include <initializer_list>

#include "raft/raft.hpp"
#include <protos/raft.pb.h>


using namespace flashpoint::raft;

namespace flashpoint::test::raft {

class InMemoryRaftManager {
 public:
  class InMemoryRaft : public Raft {
    friend InMemoryRaftManager;

   public:
    InMemoryRaft(const std::function<void(std::string)> &do_command, InMemoryRaftManager &manager, PeerId &id);

   protected:
    bool appendEntries(const PeerId &peer_id,
                       const AppendEntriesRequest &request,
                       AppendEntriesResponse &response) override;

    bool installSnapshot(const PeerId &peer_id,
                         const InstallSnapshotRequest &request,
                         InstallSnapshotResponse &response) override;

    bool requestVote(const PeerId &peer_id,
                     const RequestVoteRequest &request,
                     RequestVoteResponse &response) override;



    void registerPeer(const PeerId &peer_id, std::string peer_data) override;

    void unregisterPeer(const PeerId &peer_id) override;

   private:
    InMemoryRaftManager &manager_;
    std::set<PeerId> known_peers_ = {};
    PeerId id_;

    void appendEntries(const AppendEntriesRequest &request,
                       AppendEntriesResponse &response);

    void installSnapshot(const InstallSnapshotRequest &request,
                         InstallSnapshotResponse &response);

    void requestVote(const RequestVoteRequest &request,
                     RequestVoteResponse &response);
  };

  InMemoryRaftManager(const std::function<void(std::string)> &do_command, bool use_configs = true);

  InMemoryRaftManager(InMemoryRaftManager &&other);



  bool usingConfigs() const;

  bool allowedToContact(const PeerId &peer_a, const PeerId &peer_b);

  std::shared_ptr<InMemoryRaft> createPeer(PeerId &peer_id);

  void destroyPeer(PeerId &peer_id);



  int disconnect(const PeerId &peer_id);

  void connect(const PeerId &peer_id, int partition = 0);

  int partition(std::initializer_list<PeerId> list);

 private:
  std::function<void(std::string)> do_command_;
  std::unordered_map<PeerId, std::shared_ptr<InMemoryRaft>> rafts_ = {};
  std::unordered_map<PeerId, int> partitions_ = {};

  bool use_configs_;
};
} // namespace flashpoint::test::raft

#endif // FLASHPOINT_TEST_INCLUDE_RAFT_IN_MEMORY_HPP_
