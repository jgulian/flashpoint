#ifndef FLASHPOINT_KEYVALUE_HPP
#define FLASHPOINT_KEYVALUE_HPP

#include <grpcpp/security/server_credentials.h>
#include <grpcpp/server_builder.h>
#include <yaml-cpp/yaml.h>

#include <protos/kv.grpc.pb.h>

#include <functional>
#include <list>
#include <memory>
#include <optional>
#include <unordered_map>

#include "raft/raft.hpp"

namespace flashpoint::keyvalue {
using Operation = protos::kv::Operation;
using Status = protos::kv::Status;

struct RaftConfig {
  std::filesystem::file_time_type last_updated;
  protos::raft::Config config = {};
};

class KeyValueService;

class KeyValueServer final : public protos::kv::KeyValueApi::Service {
 private:
  friend KeyValueService;

  KeyValueService &service_;
 public:
  explicit KeyValueServer(KeyValueService &service);
  ~KeyValueServer() override = default;

  grpc::Status Get(::grpc::ServerContext *context, const ::protos::kv::GetArgs *request,
				   ::protos::kv::Operation *response) override;
  grpc::Status Put(::grpc::ServerContext *context, const ::protos::kv::PutArgs *request,
				   ::protos::kv::Operation *response) override;
  grpc::Status Cas(::grpc::ServerContext *context,
				   const ::protos::kv::CasArgs *request,
				   ::protos::kv::Operation *response) override;
};

class KeyValueService {
  using OperationResult = std::shared_ptr<std::promise<Operation>>;

 private:
  protos::kv::KeyValueState key_value_state_;
  std::map<raft::LogIndex, OperationResult> ongoing_transactions_;
  raft::LogIndex last_included_index_;

  std::unique_ptr<raft::Raft> raft_server_;
  std::unique_ptr<raft::RaftClient> raft_client_;
  std::shared_ptr<raft::RaftSettings> raft_settings_;

  std::unique_ptr<KeyValueServer> key_value_server_;

  grpc::ServerBuilder grpc_server_builder_;
  std::unique_ptr<grpc::Server> grpc_server_;

  std::unique_ptr<std::shared_mutex> lock_ = std::make_unique<std::shared_mutex>();

 public:
  explicit KeyValueService(const std::string &config_file);

  void run();
  bool update();
  void kill();

  OperationResult Put(const std::string &key, const std::string &value);
  OperationResult Get(const std::string &key);
  OperationResult Cas(const std::string &key, const std::string &expected, const std::string &updated);

 private:
  OperationResult start(Operation &operation);
  void finish(const protos::raft::LogEntry &entry);

  bool UpdateSnapshot();
};

}// namespace flashpoint::keyvalue

#endif//FLASHPOINT_KEYVALUE_HPP
