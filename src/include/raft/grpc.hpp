#ifndef FLASHPOINT_SRC_INCLUDE_RAFT_GATEWAYS_GRPC_HPP_
#define FLASHPOINT_SRC_INCLUDE_RAFT_GATEWAYS_GRPC_HPP_

#include "protos/raft.grpc.pb.h"
#include <grpcpp/create_channel.h>
#include <shared_mutex>
#include <unordered_map>

#include "raft.hpp"

namespace flashpoint::raft {

class GrpcRaft final : public Raft, public protos::raft::Raft::Service {
 public:
  explicit GrpcRaft(const PeerId &peer_id, std::function<void(std::string)> do_command,
                    std::shared_ptr<util::Logger> logger,
                    util::DefaultRandom random);

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
    GrpcPeer(const std::string& target);
    GrpcPeer(GrpcPeer&& other) noexcept;

    std::shared_ptr<grpc::Channel> channel;
    std::unique_ptr<protos::raft::Raft::Stub> stub;
    std::shared_mutex lock = {};
  };

  std::shared_mutex lock_ = {};
  std::unordered_map<PeerId, GrpcPeer> peers_;
};

}

#endif // FLASHPOINT_SRC_INCLUDE_RAFT_GATEWAYS_GRPC_HPP_
