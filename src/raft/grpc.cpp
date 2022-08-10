#include "raft/grpc.hpp"


namespace flashpoint::raft {

GrpcRaft::GrpcRaft(const PeerId &peer_id, std::function<void(Command)> do_command,
                   util::DefaultRandom random)
    : Raft(peer_id, std::move(do_command), random), lock_{} {}

bool GrpcRaft::appendEntries(const PeerId &peer_id,
                             const AppendEntriesRequest &request,
                             AppendEntriesResponse &response) {
  std::shared_lock<std::shared_mutex> lk(lock_);
  auto &peer = peers_.at(peer_id);

  grpc::ClientContext client_context;
  auto status = peer.stub->AppendEntries(&client_context, request, &response);
  if (!status.ok())
    util::LOGGER->msg(util::LogLevel::WARN, "Grpc raft failed to call client: %s", status.error_message().c_str());

  return status.ok();
}
bool GrpcRaft::installSnapshot(const PeerId &peer_id,
                               const InstallSnapshotRequest &request,
                               InstallSnapshotResponse &response) {
  std::shared_lock<std::shared_mutex> lk(lock_);
  auto &peer = peers_.at(peer_id);

  grpc::ClientContext client_context;
  auto status = peer.stub->InstallSnapshot(&client_context, request, &response);
  if (!status.ok())
    util::LOGGER->msg(util::LogLevel::WARN, "Grpc raft failed to call client: %s", status.error_message().c_str());

  return status.ok();
}
bool GrpcRaft::requestVote(const PeerId &peer_id, const RequestVoteRequest &request,
                           RequestVoteResponse &response) {
  std::shared_lock<std::shared_mutex> lk(lock_);
  auto &peer = peers_.at(peer_id);

  grpc::ClientContext client_context;
  auto status = peer.stub->RequestVote(&client_context, request, &response);
  if (!status.ok())
    util::LOGGER->msg(util::LogLevel::WARN, "Grpc raft failed to call client: %s", status.error_message().c_str());

  return status.ok();
}

void GrpcRaft::registerPeer(const PeerId &peer_id, const std::string &target) {
  std::unique_lock<std::shared_mutex> lk(lock_);

  auto peer_data = GrpcPeer(target);
  peers_.emplace(peer_id, target);
}
void GrpcRaft::unregisterPeer(const PeerId &peer_id) {
  std::unique_lock<std::shared_mutex> lk(lock_);
  peers_.erase(peer_id);
}

GrpcRaft::GrpcPeer::GrpcPeer(const std::string &target) : lock() {
  channel = grpc::CreateChannel(target, grpc::InsecureChannelCredentials());
  stub = protos::raft::Raft::NewStub(channel);
}
GrpcRaft::GrpcPeer::GrpcPeer(GrpcRaft::GrpcPeer &&other) noexcept
    : channel(std::move(other.channel)), stub(std::move(other.stub)), lock() {}

}// namespace flashpoint::raft

// namespace flashpoint::raft