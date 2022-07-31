#ifndef FLASHPOINT_TEST_INCLUDE_RAFT_IN_MEMORY_HPP_
#define FLASHPOINT_TEST_INCLUDE_RAFT_IN_MEMORY_HPP_

#include <stdarg.h>

#include <utility>
#include <unordered_map>
#include <memory>
#include <set>
#include <initializer_list>

#include "raft/raft.hpp"
#include "util/random.hpp"

#include <protos/raft.pb.h>


using namespace flashpoint::raft;

namespace flashpoint::test::raft {

class InMemoryRaftManager {
 public:
  class InMemoryRaft : public Raft {
    friend InMemoryRaftManager;

   public:
    InMemoryRaft(const PeerId &id,
                 InMemoryRaftManager &manager,
                 std::function<void(Command)> do_command,
                 std::shared_ptr<util::Logger> logger,
                 util::DefaultRandom random);

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



    void registerPeer(const PeerId &peer_id, const std::string &peer_data) override;

    void unregisterPeer(const PeerId &peer_id) override;

   private:
    void useConfig(const LogEntry &entry);

    InMemoryRaftManager &manager_;
    std::set<PeerId> known_peers_ = {};
    PeerId id_;
  };

  explicit InMemoryRaftManager(bool use_configs = true,
                               const std::shared_ptr<util::Logger> &logger = nullptr,
                               util::DefaultRandom random = {});

  InMemoryRaftManager(InMemoryRaftManager &&other) noexcept;

  ~InMemoryRaftManager();



  bool usingConfigs() const;

  bool allowedToContact(const PeerId &peer_a, const PeerId &peer_b);

  std::shared_ptr<InMemoryRaftManager::InMemoryRaft> createPeer(const PeerId &peer_id,
                                                                const std::function<void(Command)> &do_command);

  void destroyPeer(PeerId &peer_id);



  int disconnect(const PeerId &peer_id);

  void connect(const PeerId &peer_id, int partition = 0);

  int partition(std::initializer_list<PeerId> list);

  const std::unordered_map<PeerId, int> &getPartitions() const;

  static const int getDefaultPartition();

 private:
  std::unordered_map<PeerId, std::shared_ptr<InMemoryRaft>> rafts_ = {};
  std::unordered_map<PeerId, int> partitions_ = {};
  std::shared_ptr<util::Logger> logger_;
  util::DefaultRandom random_;

  bool use_configs_;
};

} // namespace flashpoint::test::raft

#endif // FLASHPOINT_TEST_INCLUDE_RAFT_IN_MEMORY_HPP_
