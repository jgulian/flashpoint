#include "raft/in_memory.hpp"

namespace flashpoint::test::raft {


InMemoryRaftManager::InMemoryRaft::InMemoryRaft(const PeerId &id,
                                                InMemoryRaftManager &manager,
                                                std::function<void(std::string)> do_command,
                                                std::shared_ptr<util::Logger> logger,
                                                util::DefaultRandom random)
    : Raft(id, std::move(do_command), std::move(logger), random), manager_(manager), id_(id) {}



bool InMemoryRaftManager::InMemoryRaft::appendEntries(const PeerId &peer_id,
                                                      const AppendEntriesRequest &request,
                                                      AppendEntriesResponse &response) {
  if (manager_.usingConfigs() && !known_peers_.contains(peer_id))
    return false;
  if (!manager_.allowedToContact(id_, peer_id))
    return false;

  manager_.rafts_[peer_id]->receiveAppendEntries(request, response);
  return true;
}

bool InMemoryRaftManager::InMemoryRaft::installSnapshot(const PeerId &peer_id,
                                                        const InstallSnapshotRequest &request,
                                                        InstallSnapshotResponse &response) {
  if (manager_.usingConfigs() && !known_peers_.contains(peer_id))
    return false;
  if (!manager_.allowedToContact(id_, peer_id))
    return false;

  manager_.rafts_[peer_id]->receiveInstallSnapshot(request, response);
  return true;
}

bool InMemoryRaftManager::InMemoryRaft::requestVote(const PeerId &peer_id,
                                                    const RequestVoteRequest &request,
                                                    RequestVoteResponse &response) {
  if (manager_.usingConfigs() && !known_peers_.contains(peer_id))
    return false;
  if (!manager_.allowedToContact(id_, peer_id))
    return false;

  manager_.rafts_[peer_id]->receiveRequestVote(request, response);
  return true;
}



void InMemoryRaftManager::InMemoryRaft::registerPeer(const PeerId &peer_id, const std::string &peer_data) {
  known_peers_.insert(peer_id);
}

void InMemoryRaftManager::InMemoryRaft::unregisterPeer(const PeerId &peer_id) {
  known_peers_.erase(peer_id);
}

void InMemoryRaftManager::InMemoryRaft::useConfig(const LogEntry &entry) {
  std::unordered_map<std::string, std::string> additions = {};
  std::unordered_set<std::string> removals = {};
  state_.configChanges(entry, additions, removals);
}



InMemoryRaftManager::InMemoryRaftManager(bool use_configs,
                                         const std::shared_ptr<util::Logger> &logger,
                                         util::DefaultRandom random)
    : use_configs_(use_configs), logger_(logger), random_(random) {}

InMemoryRaftManager::InMemoryRaftManager(InMemoryRaftManager &&other) noexcept
    : rafts_(std::move(other.rafts_)), partitions_(std::move(other.partitions_)), use_configs_(other.use_configs_), random_(other.random_) {}

InMemoryRaftManager::~InMemoryRaftManager() {
  for (auto &one : rafts_)
    for (auto &two : rafts_)
      if (one.first != two.first)
        one.second->unregisterPeer(two.first);

  for (auto &raft : rafts_)
    raft.second->forceKill();
}


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

std::shared_ptr<InMemoryRaftManager::InMemoryRaft> InMemoryRaftManager::createPeer(const PeerId &peer_id,
                                                                                   const std::function<void(std::string)> &do_command) {
  auto raft = std::make_shared<InMemoryRaft>(peer_id, *this, do_command, logger_, random_.generateRandom());
  rafts_[peer_id] = raft;
  partitions_[peer_id] = 0;


  if (!use_configs_) {
    LogEntry entry = {};
    for (const auto &raft_peer : rafts_) {
      entry.mutable_config()->mutable_entries()->insert({raft_peer.first, ""});
    }

    for (const auto &raft_peer : rafts_)
      raft_peer.second->useConfig(entry);
  }

  return raft;
}

void InMemoryRaftManager::destroyPeer(PeerId &peer_id) {
  rafts_.erase(peer_id);
  partitions_.erase(peer_id);
}

int InMemoryRaftManager::disconnect(const PeerId &peer_id) {
  if (partitions_.contains(peer_id))
    throw std::runtime_error("peer does not exist 1: " + peer_id);

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
    throw std::runtime_error("peer does not exist 2: " + peer_id);

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

const std::unordered_map<PeerId, int> &InMemoryRaftManager::getPartitions() const {
  return partitions_;
}

const int InMemoryRaftManager::getDefaultPartition() {
  return 0;
}

}