#include "keyvalue/keyvalue.hpp"

namespace flashpoint::keyvalue {

KeyValueServer::KeyValueServer(KeyValueService &service) : service_(service) {}

grpc::Status KeyValueServer::Get(::grpc::ServerContext *context, const ::protos::kv::GetArgs *request,
								 ::protos::kv::Operation *response) {
  auto operation_future = service_.Get(request->key())->get_future();
  operation_future.wait();
  response->CopyFrom(operation_future.get());
  return grpc::Status::OK;
}
grpc::Status KeyValueServer::Put(::grpc::ServerContext *context, const ::protos::kv::PutArgs *request,
								 ::protos::kv::Operation *response) {
  auto operation = service_.Put(request->key(), request->value());
  auto operation_future = operation->get_future();
  operation_future.wait();
  response->CopyFrom(operation_future.get());
  return grpc::Status::OK;
}
grpc::Status KeyValueServer::Cas(::grpc::ServerContext *context,
								 const ::protos::kv::CasArgs *request,
								 ::protos::kv::Operation *response) {
  auto operation = service_.Cas(request->key(), request->expected(), request->updated());
  auto operation_future = operation->get_future();
  operation_future.wait();
  response->CopyFrom(operation_future.get());
  return grpc::Status::OK;
}

KeyValueService::KeyValueService(const std::string &config_file) : last_included_index_(0) {
  auto config = YAML::LoadFile(config_file);
  if (!config["raft_config"] || !config["raft_config"].IsMap() || !config["me"] || !config["host_address"])
	throw std::runtime_error("config file not formatted correctly.");

  protos::raft::Config starting_config = {};
  protos::raft::Peer me = {};
  auto peers = config["raft_config"];
  for (YAML::const_iterator it = peers.begin(); it != peers.end(); ++it) {
	auto id = it->first.as<std::string>();
	for (auto &kPeer : starting_config.peers())
	  if (kPeer.id() == id)
		throw std::runtime_error("peers must have different ids");

	auto peer = starting_config.mutable_peers()->Add();
	peer->set_id(id);
	peer->set_voting(true);
	peer->mutable_data()->set_address(it->second.as<std::string>());

	if (id == config["me"].as<std::string>())
	  me.CopyFrom(*peer);
  }

  if (me.id() != config["me"].as<std::string>())
	throw std::runtime_error("me must be an id used in the config");

  raft_settings_ = std::make_shared<raft::RaftSettings>();
  raft_settings_->me = me;
  raft_settings_->starting_config = starting_config;
  raft_settings_->apply_command = [this](auto &&entry) { Finish(std::forward<decltype(entry)>(entry)); };
  raft_settings_->apply_config_update = [this](auto &&entry) {
	auto config = protos::raft::Config();
	config.ParseFromString(entry.log_data().data());
	raft_client_->UpdateConfig(std::forward<protos::raft::Config>(config));
  };

  if (auto persistence_config = config["persistence"]) {
	if (!persistence_config["persistent_file"] || !persistence_config["persist_threshold"]
		|| !persistence_config["snapshot_file"])
	  throw std::runtime_error("config file not formatted correctly.");

	util::GetLogger()->Log("persistence enabled");
	raft_settings_->persistence_settings.emplace();
	raft_settings_->persistence_settings.value().snapshot_file = persistence_config["snapshot_file"].as<std::string>();
	raft_settings_->persistence_settings.value().persistent_file =
		persistence_config["persistent_file"].as<std::string>();
	raft_settings_->persistence_settings->persistence_threshold =
		persistence_config["persist_threshold"].as<unsigned long long>();
  }

  raft_server_ = std::make_unique<raft::Raft>(raft_settings_);
  raft_client_ = std::make_unique<raft::RaftClient>(me.id(), starting_config);
  key_value_server_ = std::make_unique<KeyValueServer>(*this);

  auto &kRaftHostAddress = me.data().address();
  const auto kKvHostAddress = config["host_address"].as<std::string>();

  grpc_server_builder_.AddListeningPort(kKvHostAddress, grpc::InsecureServerCredentials());
  grpc_server_builder_.AddListeningPort(kRaftHostAddress, grpc::InsecureServerCredentials());
  grpc_server_builder_.RegisterService(kKvHostAddress, key_value_server_.get());
  grpc_server_builder_.RegisterService(kRaftHostAddress, raft_server_.get());
}

void KeyValueService::Run() {
  grpc_server_ = grpc_server_builder_.BuildAndStart();
  while (!raft_server_->Run());
}
bool KeyValueService::Update() {
  if (raft_settings_->persistence_settings.has_value()) {
	auto recent_persist = raft_settings_->persistence_settings->recent_persists.TryPop();
    bool needs_snapshot = false;
	while (recent_persist.has_value()) {
	  needs_snapshot |= raft_settings_->persistence_settings->persistence_threshold < recent_persist.value();
	  recent_persist = raft_settings_->persistence_settings->recent_persists.Pop();
	}

	if (needs_snapshot)
	  UpdateSnapshot();
  }
  return true;
}
void KeyValueService::Kill() {}

KeyValueService::OperationResult KeyValueService::Put(const std::string &key, const std::string &value) {
  auto op = Operation();
  op.mutable_put()->mutable_args()->set_key(key);
  op.mutable_put()->mutable_args()->set_value(value);
  return Start(op);
}
KeyValueService::OperationResult KeyValueService::Get(const std::string &key) {
  auto op = Operation();
  op.mutable_get()->mutable_args()->set_key(key);
  return Start(op);
}
KeyValueService::OperationResult KeyValueService::Cas(const std::string &key,
													  const std::string &expected,
													  const std::string &updated) {
  auto op = Operation();
  op.mutable_cas()->mutable_args()->set_key(key);
  op.mutable_cas()->mutable_args()->set_expected(expected);
  op.mutable_cas()->mutable_args()->set_updated(updated);
  return Start(op);
}

KeyValueService::OperationResult KeyValueService::Start(Operation &operation) {
  auto log_index = raft_client_->Start(operation.SerializeAsString());
  auto operation_result = std::make_shared<std::promise<Operation>>();
  ongoing_transactions_[log_index] = operation_result;
  return operation_result;
}
void KeyValueService::Finish(const protos::raft::LogEntry &entry) {
  Operation operation = {};
  operation.ParseFromString(entry.log_data().data());

  switch (operation.data_case()) {
	case Operation::kGet: {
	  auto &kKey = operation.get().args().key();
	  if (kKey.empty() || !key_value_state_.data().contains(kKey)) {
		operation.mutable_status()->set_code(protos::kv::KeyNotFound);
		operation.mutable_status()->set_info(kKey + " not found");
		break;
	  }
	  operation.mutable_get()->mutable_reply()->set_value(key_value_state_.data().at(kKey));
	  operation.mutable_status()->set_code(protos::kv::Ok);
	  break;
	}
	case Operation::kPut:
	  if (operation.put().args().key().empty()) {
		operation.mutable_status()->set_code(protos::kv::InvalidOperation);
		operation.mutable_status()->set_info("key is empty");
		break;
	  }
	  (*key_value_state_.mutable_data())[operation.put().args().key()] = operation.put().args().value();
	  operation.mutable_status()->set_code(protos::kv::Ok);
	  break;
	default: {
	  operation.mutable_status()->set_code(protos::kv::InvalidOperation);
	  operation.mutable_status()->set_info("operation data not set");
	  break;
	}
	case Operation::kCas: {
	  auto &kKey = operation.cas().args().key();
	  if (kKey.empty()) {
		operation.mutable_status()->set_code(protos::kv::InvalidOperation);
		operation.mutable_status()->set_info("key is empty");
		break;
	  }
	  if (!key_value_state_.data().contains(kKey)) {
		operation.mutable_status()->set_code(protos::kv::KeyNotFound);
		operation.mutable_status()->set_info(kKey + " not found");
		break;
	  }
	  auto &kActual = key_value_state_.data().at(kKey);
	  auto &kExpected = operation.cas().args().expected();
	  operation.mutable_cas()->mutable_reply()->set_actual(kActual);

	  if (kActual == kExpected) {
		key_value_state_.mutable_data()->at(kKey) = operation.cas().args().updated();
		operation.mutable_status()->set_code(protos::kv::Ok);
	  } else {
		operation.mutable_status()->set_code(protos::kv::UnexpectedValue);
		operation.mutable_status()->set_info("expected: " + kExpected + "\nactual: " + kActual);
	  }
	}
  }

  {
	std::unique_lock lock(lock_);
	if (ongoing_transactions_.contains(entry.index())) {
	  auto operation_result = ongoing_transactions_[entry.index()];
	  operation_result->set_value(operation);
	  ongoing_transactions_.erase(entry.index());
	}
	last_included_index_ = entry.index();
  }
}

bool KeyValueService::UpdateSnapshot() {
  if (!raft_settings_->persistence_settings.has_value())
	throw std::runtime_error("can't Update Snapshot when snapshots are not enabled");

  auto temp_file_name = "~" + raft_settings_->persistence_settings->snapshot_file;
  std::unique_lock lock(lock_);

  if (std::filesystem::exists(temp_file_name))
	return false;

  std::ofstream temp_file(temp_file_name);
  key_value_state_.SerializeToOstream(&temp_file);
  temp_file.close();

  std::ifstream temp_file_read(temp_file_name);
  std::ofstream real_file(raft_settings_->persistence_settings->snapshot_file);
  real_file << temp_file_read.rdbuf();
  std::remove(temp_file_name.c_str());

  util::GetLogger()->Log("starting new Snapshot");
  return raft_server_->Snapshot(last_included_index_);
}

}// namespace flashpoint::keyvalue