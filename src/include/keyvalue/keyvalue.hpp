#ifndef FLASHPOINT_KEYVALUE_HPP
#define FLASHPOINT_KEYVALUE_HPP

#include "yaml-cpp/yaml.h"
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

  explicit KeyValueServer(KeyValueService &service);

 public:
  ~KeyValueServer() override;
  grpc::Status Get(::grpc::ServerContext *context, const ::protos::kv::GetArgs *request,
                   ::protos::kv::Operation *response) override;
  grpc::Status Put(::grpc::ServerContext *context, const ::protos::kv::PutArgs *request,
                   ::protos::kv::Operation *response) override;
};

class KeyValueService {
  using OperationResult = std::shared_ptr<std::promise<Operation>>;

 private:
  std::unordered_map<std::string, std::string> data_;
  raft::RaftClient client_;
  RaftConfig raft_config_ = {};
  std::unique_ptr<std::thread> service_updater_;
  std::unique_ptr<std::shared_mutex> lock_;
  std::map<raft::LogIndex, OperationResult> ongoing_transactions_;

 public:
  KeyValueService(const std::string &address, const std::string &config_file);

  OperationResult start(Operation &operation);
  void finish(const protos::raft::LogEntry &entry);

  OperationResult put(const std::string &key, const std::string &value);
  OperationResult get(const std::string &key);
};

}// namespace flashpoint::keyvalue

#endif//FLASHPOINT_KEYVALUE_HPP
