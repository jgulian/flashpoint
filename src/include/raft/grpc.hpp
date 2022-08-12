#ifndef FLASHPOINT_SRC_INCLUDE_RAFT_GATEWAYS_GRPC_HPP_
#define FLASHPOINT_SRC_INCLUDE_RAFT_GATEWAYS_GRPC_HPP_

#include "protos/raft.grpc.pb.h"
#include <grpcpp/create_channel.h>
#include <shared_mutex>
#include <unordered_map>

#include "raft.hpp"
#include "raft/join_grpc.hpp"

namespace flashpoint::raft {

class GrpcRaft final : public Raft, public protos::raft::Raft::Service {
 public:
  GrpcRaft(const PeerId &peer_id, std::function<void(Command)> do_command,
           util::DefaultRandom random);

  GrpcRaft(const PeerId &peer_id, const protos::raft::Config &config, std::function<void(Command)> do_command,
           util::DefaultRandom random);

  grpc::Status AppendEntries(::grpc::ServerContext *context, const ::protos::raft::AppendEntriesRequest *request, ::protos::raft::AppendEntriesResponse *response) override;
  grpc::Status RequestVote(::grpc::ServerContext *context, const ::protos::raft::RequestVoteRequest *request, ::protos::raft::RequestVoteResponse *response) override;
  grpc::Status InstallSnapshot(::grpc::ServerContext *context, const ::protos::raft::InstallSnapshotRequest *request, ::protos::raft::InstallSnapshotResponse *response) override;
  grpc::Status JoinCluster(::grpc::ServerContext *context, const ::protos::raft::JoinClusterRequest *request, ::protos::raft::JoinClusterResponse *response) override;

 protected:
  bool appendEntries(const PeerId &peer_id, const AppendEntriesRequest &request,
                     AppendEntriesResponse &response) override;
  bool installSnapshot(const PeerId &peer_id, const InstallSnapshotRequest &request,
                       InstallSnapshotResponse &response) override;
  bool requestVote(const PeerId &peer_id, const RequestVoteRequest &request,
                   RequestVoteResponse &response) override;

  void registerPeer(const PeerId &peer_id, const std::string &peer_data) override;
  void unregisterPeer(const PeerId &peer_id) override;

private:
  struct GrpcPeer {
    explicit GrpcPeer(const std::string &target);
    GrpcPeer(GrpcPeer &&other) noexcept;

    std::shared_ptr<grpc::Channel> channel;
    std::unique_ptr<protos::raft::Raft::Stub> stub;
    std::shared_mutex lock = {};
  };

  std::unique_ptr<std::shared_mutex> lock_ = {};
  std::unordered_map<PeerId, GrpcPeer> peers_;
};

}

#endif // FLASHPOINT_SRC_INCLUDE_RAFT_GATEWAYS_GRPC_HPP_
