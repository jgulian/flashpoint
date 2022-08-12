#include "raft/grpc.hpp"


namespace flashpoint::raft {

GrpcRaft::GrpcRaft(const PeerId &peer_id, std::function<void(Command)> do_command,
                   util::DefaultRandom random)
    : Raft(peer_id, std::move(do_command), random), lock_{} {}

GrpcRaft::GrpcRaft(const PeerId &peer_id, const protos::raft::Config &config, std::function<void(Command)> do_command,
                   util::DefaultRandom random)
    : Raft(peer_id, config, std::move(do_command), random), lock_{} {}

grpc::Status GrpcRaft::AppendEntries(::grpc::ServerContext *context, const ::protos::raft::AppendEntriesRequest *request, ::protos::raft::AppendEntriesResponse *response) {
  appendEntries(request->leader_id(), *request, *response);
  return grpc::Status::OK;
}
grpc::Status GrpcRaft::RequestVote(::grpc::ServerContext *context, const ::protos::raft::RequestVoteRequest *request, ::protos::raft::RequestVoteResponse *response) {
  requestVote(request->candidate_id(), *request, *response);
  return grpc::Status::OK;
}
grpc::Status GrpcRaft::InstallSnapshot(::grpc::ServerContext *context, const ::protos::raft::InstallSnapshotRequest *request, ::protos::raft::InstallSnapshotResponse *response) {
  installSnapshot(request->leader_id(), *request, *response);
  return grpc::Status::OK;
}

grpc::Status GrpcRaft::JoinCluster(::grpc::ServerContext *context, const ::protos::raft::JoinClusterRequest *request, ::protos::raft::JoinClusterResponse *response) {
  std::shared_lock<std::shared_mutex> lk(*lock_);

  auto read_lock = state_->acquireReadLock();
  auto leader_id = getLeaderId();
  auto me = getId();
  if (leader_id != me) {
    response->set_successful(false);
    response->mutable_leader()->CopyFrom(state_->findPeer(leader_id));
  } else {
    bool ok = handleJoinClusterStageOne(*request, *response);
    if (ok) {
      start
    }
  }

  util::LOGGER->log("received requst to join cluster");

  return grpc::Status::OK;
}

bool GrpcRaft::appendEntries(const PeerId &peer_id,
                             const AppendEntriesRequest &request,
                             AppendEntriesResponse &response) {
  std::shared_lock<std::shared_mutex> lk(*lock_);
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
  std::shared_lock<std::shared_mutex> lk(*lock_);
  auto &peer = peers_.at(peer_id);

  grpc::ClientContext client_context;
  auto status = peer.stub->InstallSnapshot(&client_context, request, &response);
  if (!status.ok())
    util::LOGGER->msg(util::LogLevel::WARN, "Grpc raft failed to call client: %s", status.error_message().c_str());

  return status.ok();
}
bool GrpcRaft::requestVote(const PeerId &peer_id, const RequestVoteRequest &request,
                           RequestVoteResponse &response) {
  std::shared_lock<std::shared_mutex> lk(*lock_);
  auto &peer = peers_.at(peer_id);

  grpc::ClientContext client_context;
  auto status = peer.stub->RequestVote(&client_context, request, &response);
  if (!status.ok())
    util::LOGGER->msg(util::LogLevel::WARN, "Grpc raft failed to call client: %s", status.error_message().c_str());

  return status.ok();
}

void GrpcRaft::registerPeer(const PeerId &peer_id, const std::string &target) {
  std::unique_lock<std::shared_mutex> lk(*lock_);

  auto peer_data = GrpcPeer(target);
  peers_.emplace(peer_id, target);
}
void GrpcRaft::unregisterPeer(const PeerId &peer_id) {
  std::unique_lock<std::shared_mutex> lk(*lock_);
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