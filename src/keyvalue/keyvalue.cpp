#include "keyvalue/keyvalue.hpp"

namespace flashpoint::keyvalue {


KeyValueServer::KeyValueServer(KeyValueService &service) : service_(service) {}
KeyValueServer::~KeyValueServer() { throw std::runtime_error("not completed"); }

grpc::Status KeyValueServer::Get(::grpc::ServerContext *context, const ::protos::kv::GetArgs *request,
                                 ::protos::kv::Operation *response) {
  auto operation_future = service_.get(request->key())->get_future();
  operation_future.wait();
  response->CopyFrom(operation_future.get());
  return grpc::Status::OK;
}
grpc::Status KeyValueServer::Put(::grpc::ServerContext *context, const ::protos::kv::PutArgs *request,
                                 ::protos::kv::Operation *response) {
  auto operation_future = service_.put(request->key(), request->value())->get_future();
  operation_future.wait();
  response->CopyFrom(operation_future.get());
  return grpc::Status::OK;
}

KeyValueService::KeyValueService(const std::string &address, const std::string &config_file) {
  auto config = YAML::LoadFile(config_file);
  if (!config["raft_config"] || !config["raft_config"].IsMap() || !config["me"] || !config["raft_address"])
    throw std::runtime_error("config file not formatted correctly.");

  protos::raft::Config starting_config = {};
  protos::raft::Peer me = {};
  auto peers = config["raft_config"];
  for (YAML::const_iterator it = peers.begin(); it != peers.end(); ++it) {
    auto id = it->first.as<std::string>();
    for (auto &peer : starting_config.peers())
      if (peer.id() == id) throw std::runtime_error("peers must have different ids");

    auto peer = starting_config.mutable_peers()->Add();
    peer->set_id(id);
    peer->set_voting(true);
    peer->mutable_data()->set_address(it->second.as<std::string>());
    // TODO: Update for other connection options.

    if (id == config["me"].as<std::string>()) me.CopyFrom(*peer);
  }

  if (me.id() != config["me"].as<std::string>()) throw std::runtime_error("me must be an id used in the config");

  auto raft_config = std::make_unique<raft::RaftConfig>();
  raft_config->me = me;
  raft_config->starting_config = starting_config;
  raft_config->apply_command = [this](auto &&entry) { finish(std::forward<decltype(entry)>(entry)); };
  raft_config->apply_config_update = [this](auto &&entry) {
    auto config = protos::raft::Config();
    config.ParseFromString(entry.log_data().data());
    raft_client_->updateConfig(std::forward<protos::raft::Config>(config));
  };

  raft_server_ = std::make_unique<raft::Raft>(std::move(raft_config));
  raft_client_ = std::make_unique<raft::RaftClient>(starting_config);
  key_value_server_ = std::make_unique<KeyValueServer>(*this);

  auto raft_host_address = config["raft_address"].as<std::string>();

  grpc_server_builder_.AddListeningPort(address, grpc::InsecureServerCredentials());
  grpc_server_builder_.AddListeningPort(raft_host_address, grpc::InsecureServerCredentials());
  grpc_server_builder_.RegisterService(address, key_value_server_.get());
  grpc_server_builder_.RegisterService(raft_host_address, raft_server_.get());
}

void KeyValueService::run() {
  grpc_server_ = grpc_server_builder_.BuildAndStart();
  while (!raft_server_->run())
    ;
}
bool KeyValueService::update() { return true; }
void KeyValueService::kill() {}

KeyValueService::OperationResult KeyValueService::start(Operation &operation) {
  auto log_index = raft_client_->start(operation.SerializeAsString());
  auto operation_result = std::make_shared<std::promise<Operation>>();
  ongoing_transactions_[log_index] = operation_result;
  return operation_result;
}
void KeyValueService::finish(const protos::raft::LogEntry &entry) {
  Operation operation = {};
  operation.ParseFromString(entry.log_data().data());

  switch (operation.data_case()) {
    case Operation::kGet: {
      auto &key = operation.put().args().key();
      if (key.empty() || !data_.contains(key)) {
        operation.mutable_status()->set_code(protos::kv::KeyNotFound);
        operation.mutable_status()->set_info(key + " not found");
        break;
      }
      operation.mutable_get()->mutable_reply()->set_value(data_.at(key));
      operation.mutable_status()->set_code(protos::kv::Ok);
      break;
    }
    case Operation::kPut:
      if (operation.put().args().key().empty()) {
        operation.mutable_status()->set_code(protos::kv::InvalidOperation);
        operation.mutable_status()->set_info("key is empty");
        break;
      }
      data_[operation.put().args().key()] = operation.put().args().value();
      operation.mutable_status()->set_code(protos::kv::Ok);
      break;
    case Operation::DATA_NOT_SET:
      operation.mutable_status()->set_code(protos::kv::InvalidOperation);
      operation.mutable_status()->set_info("operation data not set");
      break;
  }

  {
    std::unique_lock lock(*lock_);
    if (ongoing_transactions_.contains(entry.index())) {
      auto operation_result = ongoing_transactions_[entry.index()];
      operation_result->set_value(operation);
      ongoing_transactions_.erase(entry.index());
    }
  }
}

KeyValueService::OperationResult KeyValueService::put(const std::string &key, const std::string &value) {
  auto op = Operation();
  op.mutable_put()->mutable_args()->set_key(key);
  op.mutable_put()->mutable_args()->set_value(value);
  return start(op);
}

KeyValueService::OperationResult KeyValueService::get(const std::string &key) {
  auto op = Operation();
  op.mutable_get()->mutable_args()->set_key(key);
  return start(op);
}
}// namespace flashpoint::keyvalue