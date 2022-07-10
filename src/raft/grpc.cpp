#include "raft/grpc.hpp"

namespace flashpoint::raft {

/*
bool GrpcProtocol::appendEntries(PeerId peer_id,
                                 const AppendEntriesRequest &request,
                                 AppendEntriesResponse &response) {
  std::shared_lock<std::shared_mutex> lk(lock_);

  if (!peers_.contains(peer_id))
    throw std::runtime_error("no such peer with peer id");

  grpc::ClientContext context = {};
  auto status =
      peers_.at(peer_id).stub->AppendEntries(&context, request, &response);

  return status.ok();
}
bool GrpcProtocol::installSnapshot(PeerId peer_id,
                                   const InstallSnapshotRequest &request,
                                   InstallSnapshotResponse &response) {
  std::shared_lock<std::shared_mutex> lk(lock_);

  if (!peers_.contains(peer_id))
    throw std::runtime_error("no such peer with peer id");

  grpc::ClientContext context = {};
  auto status =
      peers_.at(peer_id).stub->InstallSnapshot(&context, request, &response);

  return status.ok();
}
bool GrpcProtocol::requestVote(PeerId peer_id,
                               const RequestVoteRequest &request,
                               RequestVoteResponse &response) {
  std::shared_lock<std::shared_mutex> lk(lock_);

  if (!peers_.contains(peer_id))
    throw std::runtime_error("no such peer with peer id");

  grpc::ClientContext context = {};
  auto status =
      peers_.at(peer_id).stub->RequestVote(&context, request, &response);

  return status.ok();
}
PeerId GrpcProtocol::registerPeer(std::string peer_data) {
  std::unique_lock<std::shared_mutex> lk(lock_);

  PeerId peer_id = current_peer_id_++;
  auto peer = GrpcProtocol::Peer(peer_data);
  peers_.insert({peer_id, std::move(peer)});
  return peer_id;
}
void GrpcProtocol::unregisterPeer(PeerId peer_id) {
  std::unique_lock<std::shared_mutex> lk(lock_);

  if (!peers_.contains(peer_id))
    throw std::runtime_error("no such peer with peer id");

  peers_.erase(peer_id);
}

GrpcProtocol::Peer::Peer(std::string &target) {
}
GrpcProtocol::Peer::Peer(Peer &&peer) noexcept
    : lock(), channel(std::move(peer.channel)), stub(std::move(peer.stub)) {}

 */

GrpcRaft::GrpcRaft(std::function<void(std::string)> do_command)
    : Raft(std::move(do_command)), lock_{} {}

bool GrpcRaft::appendEntries(const PeerId &peer_id,
                             const AppendEntriesRequest &request,
                             AppendEntriesResponse &response) {
  std::shared_lock<std::shared_mutex> lk(lock_);

  return false;
}
bool GrpcRaft::installSnapshot(const PeerId &peer_id,
                               const InstallSnapshotRequest &request,
                               InstallSnapshotResponse &response) {
  std::shared_lock<std::shared_mutex> lk(lock_);

  return false;
}
bool GrpcRaft::requestVote(const PeerId &peer_id, const RequestVoteRequest &request,
                           RequestVoteResponse &response) {
  std::shared_lock<std::shared_mutex> lk(lock_);

  return false;
}

void GrpcRaft::registerPeer(const PeerId &peer_id, std::string target) {
  std::unique_lock<std::shared_mutex> lk(lock_);

  auto peer_data = GrpcPeer(target);
  peers_.emplace(peer_id, target);
}
void GrpcRaft::unregisterPeer(const PeerId &peer_id) {
  std::unique_lock<std::shared_mutex> lk(lock_);
}

GrpcRaft::GrpcPeer::GrpcPeer(const std::string &target) : lock() {
  channel = grpc::CreateChannel(target, grpc::InsecureChannelCredentials());
  stub = protos::raft::Raft::NewStub(channel);
}
GrpcRaft::GrpcPeer::GrpcPeer(GrpcRaft::GrpcPeer &&other) noexcept
    : channel(std::move(other.channel)), stub(std::move(other.stub)), lock() {}

} // namespace flashpoint::raft