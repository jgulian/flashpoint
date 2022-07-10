#include "raft/in_memory.hpp"

namespace flashpoint::test::raft {


InMemoryRaftManager::InMemoryRaft::InMemoryRaft(const std::function<void(std::string)> &do_command,
                                                InMemoryRaftManager &manager,
                                                PeerId &id)
    : Raft(do_command), manager_(manager), id_(id) {}



bool InMemoryRaftManager::InMemoryRaft::appendEntries(const PeerId &peer_id,
                                                      const AppendEntriesRequest &request,
                                                      AppendEntriesResponse &response) {
  if (manager_.usingConfigs() && !known_peers_.contains(peer_id))
    return false;
  if (!manager_.allowedToContact(id_, peer_id))
    return false;

  manager_.rafts_[peer_id]->appendEntries(request, response);
  return true;
}

bool InMemoryRaftManager::InMemoryRaft::installSnapshot(const PeerId &peer_id,
                                                        const InstallSnapshotRequest &request,
                                                        InstallSnapshotResponse &response) {
  if (manager_.usingConfigs() && !known_peers_.contains(peer_id))
    return false;
  if (!manager_.allowedToContact(id_, peer_id))
    return false;

  manager_.rafts_[peer_id]->installSnapshot(request, response);
  return true;
}

bool InMemoryRaftManager::InMemoryRaft::requestVote(const PeerId &peer_id,
                                                    const RequestVoteRequest &request,
                                                    RequestVoteResponse &response) {
  if (manager_.usingConfigs() && !known_peers_.contains(peer_id))
    return false;
  if (!manager_.allowedToContact(id_, peer_id))
    return false;

  manager_.rafts_[peer_id]->requestVote(request, response);
  return true;
}



void InMemoryRaftManager::InMemoryRaft::registerPeer(const PeerId &peer_id, std::string peer_data) {
  known_peers_.insert(peer_id);
}

void InMemoryRaftManager::InMemoryRaft::unregisterPeer(const PeerId &peer_id) {
  known_peers_.erase(peer_id);
}



void InMemoryRaftManager::InMemoryRaft::appendEntries(const AppendEntriesRequest &request,
                                                      AppendEntriesResponse &response) {
  receiveAppendEntries(request, response);
}

void InMemoryRaftManager::InMemoryRaft::installSnapshot(const InstallSnapshotRequest &request,
                                                        InstallSnapshotResponse &response) {
  receiveInstallSnapshot(request, response);
}

void InMemoryRaftManager::InMemoryRaft::requestVote(const RequestVoteRequest &request,
                                                    RequestVoteResponse &response) {
  receiveRequestVote(request, response);
}



InMemoryRaftManager::InMemoryRaftManager(const std::function<void(std::string)> &do_command, bool use_configs)
    : do_command_(std::move(do_command)), use_configs_(use_configs) {}

InMemoryRaftManager::InMemoryRaftManager(InMemoryRaftManager &&other)
    : do_command_(std::move(other.do_command_)), rafts_(std::move(other.rafts_)),
      partitions_(std::move(other.partitions_)), use_configs_(other.use_configs_) {}



bool InMemoryRaftManager::usingConfigs() const {
  return use_configs_;
}

bool InMemoryRaftManager::allowedToContact(const PeerId &peer_a, const PeerId &peer_b) {
  if (!partitions_.contains(peer_a))
    return false;
  if (!partitions_.contains(peer_b))
    return false;
  return partitions_[peer_a] == partitions_[peer_b];
}

std::shared_ptr<InMemoryRaftManager::InMemoryRaft> InMemoryRaftManager::createPeer(PeerId &peer_id) {
  auto raft = std::make_shared<InMemoryRaft>(do_command_, *this, peer_id);
  rafts_[peer_id] = raft;
  partitions_[peer_id] = 0;
  return raft;
}

void InMemoryRaftManager::destroyPeer(PeerId &peer_id) {
  rafts_.erase(peer_id);
  partitions_.erase(peer_id);
}

int InMemoryRaftManager::disconnect(const PeerId &peer_id) {
  if (partitions_.contains(peer_id))
    throw std::runtime_error("peer does not exist");

  std::unordered_set<int> partitions = {};
  for (const auto &peer : partitions_)
    partitions.insert(peer.second);

  int new_partition_id = 1;
  while (partitions.contains(new_partition_id))
    new_partition_id++;

  partitions_[peer_id] = new_partition_id;
  return new_partition_id;
}

void InMemoryRaftManager::connect(const PeerId &peer_id, int partition) {
  if (partitions_.contains(peer_id))
    throw std::runtime_error("peer does not exist");

  partitions_[peer_id] = partition;
}

int InMemoryRaftManager::partition(std::initializer_list<PeerId> list) {
  int partition = -1;
  for (auto &peer : list) {
    if (partition == -1)
      partition = disconnect(peer);
    else
      connect(peer, partition);
  }
  return partition;
}
}