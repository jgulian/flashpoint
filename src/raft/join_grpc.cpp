#include "raft/join_grpc.hpp"

namespace flashpoint::raft {

class JoinRaftService : public protos::raft::Raft::Service {
 private:
  std::atomic<bool> &failed_;
  std::atomic<int> expected_append_entries_term_ = 0;

 public:
  explicit JoinRaftService(std::atomic<bool> &failed) : failed_(failed) {}

  grpc::Status AppendEntries(::grpc::ServerContext *context, const ::protos::raft::AppendEntriesRequest *request, ::protos::raft::AppendEntriesResponse *response) override {
    failed_ = failed_ || expected_append_entries_term_ != request->term();
    response->set_success(!failed_);
    return grpc::Status::OK;
  }
  grpc::Status RequestVote(::grpc::ServerContext *context, const ::protos::raft::RequestVoteRequest *request, ::protos::raft::RequestVoteResponse *response) override {
    failed_ = true;
    return grpc::Status::OK;
  }
  grpc::Status InstallSnapshot(::grpc::ServerContext *context, const ::protos::raft::InstallSnapshotRequest *request, ::protos::raft::InstallSnapshotResponse *response) override {
    failed_ = true;
    return grpc::Status::OK;
  }
  grpc::Status JoinCluster(::grpc::ServerContext *context, const ::protos::raft::JoinClusterRequest *request, ::protos::raft::JoinClusterResponse *response) override {
    failed_ = true;
    return grpc::Status::OK;
  }
};

struct JoinServerData {
  std::unique_ptr<JoinRaftService> service;
  std::unique_ptr<grpc::Server> grpc_server;
};

struct PeerInfo {
  std::string host_address;
};

PeerInfo parsePeerInfoFromData(const std::string &peer_data) {
  return {peer_data};
}

JoinServerData generateJoinGrpcServer(const std::string &host_address, std::atomic<bool> &failed) {
  auto client = std::make_unique<JoinRaftService>(failed);
  grpc::ServerBuilder server_builder;
  server_builder.AddListeningPort(host_address, grpc::InsecureServerCredentials());
  server_builder.RegisterService(client.get());
  std::unique_ptr<grpc::Server> server(server_builder.BuildAndStart());
  return {std::move(client), std::move(server)};
}

struct LeaderConnection {
  enum {
    NONE,
    ClientCouldNotConnect,
    LeaderCouldNotConnect,
    Other,
  } failure;
  std::unique_ptr<protos::raft::Raft::Stub> stub = nullptr;
  std::optional<protos::raft::JoinClusterResponse::ClusterInfo> config = std::nullopt;
};

// Might cause issue if NewStub does not copy channel pointer.
LeaderConnection connectToClusterLeader(const std::string &candidate_data, const std::string &known_peer_data, const std::atomic<bool> &failed) {
  auto channel = grpc::CreateChannel(known_peer_data, grpc::InsecureChannelCredentials());
  auto stub = protos::raft::Raft::NewStub(channel);

  protos::raft::JoinClusterRequest request = {};
  request.set_candidate_data(candidate_data);

  while (!failed) {
    grpc::ClientContext client_context = {};
    protos::raft::JoinClusterResponse response = {};
    grpc::Status status;

    try {
      status = stub->JoinCluster(&client_context, request, &response);
    } catch (...) {
      return {LeaderConnection::ClientCouldNotConnect};
    }

    if (!status.ok())
      return {LeaderConnection::ClientCouldNotConnect};
    switch (response.data_case()) {
      case JoinClusterResponse::kLeader: {
        auto peer_data = parsePeerInfoFromData(response.leader().data());
        channel = grpc::CreateChannel(peer_data.host_address, grpc::InsecureChannelCredentials());
        stub = protos::raft::Raft::NewStub(channel);
      } break;
      case JoinClusterResponse::kInfo: {
        if (!response.successful())
          return {LeaderConnection::LeaderCouldNotConnect};
        return {LeaderConnection::NONE, std::move(stub), response.info()};
      } break;
      case JoinClusterResponse::DATA_NOT_SET:
        return {LeaderConnection::Other};
    }
  }

  return {LeaderConnection::Other};
}

bool joinRaftGrpc(const std::string &host_address, const std::string &known_peer_data, const std::function<GrpcRaft &()> &start_raft) {
  const auto &peer_address = known_peer_data;
  std::atomic<bool> failed = false;

  auto local_join_server = generateJoinGrpcServer(host_address, failed);
  auto cluster_data = connectToClusterLeader(host_address, known_peer_data, failed);
  if (failed || cluster_data.failure != LeaderConnection::NONE)
    return false;

  auto &raft = start_raft();
  raft.run();
  while (raft.running() && !raft.canVote())
    std::this_thread::yield();

  return raft.running();
}

bool handleJoinClusterStageOne(const JoinClusterRequest &request, JoinClusterResponse &response) {
  auto candidate_info = parsePeerInfoFromData(request.candidate_data());

  auto channel = grpc::CreateChannel(candidate_info.host_address, grpc::InsecureChannelCredentials());
  auto stub = protos::raft::Raft::NewStub(channel);

  response.set_successful(false);

  try {
    grpc::ClientContext client_context = {};
    AppendEntriesRequest ae_request = {};
    ae_request.set_term(0);
    AppendEntriesResponse ae_response = {};
    auto status = stub->AppendEntries(&client_context, ae_request, &ae_response);
    if (!status.ok())
      return false;
  } catch (...) {
    return false;
  }

  response.set_successful(true);
  return true;
}
}// namespace flashpoint::raft