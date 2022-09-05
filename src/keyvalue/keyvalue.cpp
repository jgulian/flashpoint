#include "keyvalue/keyvalue.hpp"

namespace flashpoint::keyvalue {

KeyValueServer::KeyValueServer(KeyValueService &service) : service_(service) {}

grpc::Status KeyValueServer::Get(::grpc::ServerContext *context, const ::protos::kv::GetArgs *request,
								 ::protos::kv::Operation *response) {
  auto operation_future = service_.get(request->key())->get_future();
  operation_future.wait();
  response->CopyFrom(operation_future.get());
  return grpc::Status::OK;
}
grpc::Status KeyValueServer::Put(::grpc::ServerContext *context, const ::protos::kv::PutArgs *request,
								 ::protos::kv::Operation *response) {
  auto operation = service_.put(request->key(), request->value());
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
	for (auto &peer : starting_config.peers())
	  if (peer.id() == id)
		throw std::runtime_error("peers must have different ids");

	auto peer = starting_config.mutable_peers()->Add();
	peer->set_id(id);
	peer->set_voting(true);
	peer->mutable_data()->set_address(it->second.as<std::string>());
	// TODO: Update for other connection options.

	if (id == config["me"].as<std::string>())
	  me.CopyFrom(*peer);
  }

  if (me.id() != config["me"].as<std::string>())
	throw std::runtime_error("me must be an id used in the config");

  raft_settings_ = std::make_shared<raft::RaftSettings>();
  raft_settings_->me = me;
  raft_settings_->starting_config = starting_config;
  raft_settings_->apply_command = [this](auto &&entry) { finish(std::forward<decltype(entry)>(entry)); };
  raft_settings_->apply_config_update = [this](auto &&entry) {
	auto config = protos::raft::Config();
	config.ParseFromString(entry.log_data().data());
	raft_client_->updateConfig(std::forward<protos::raft::Config>(config));
  };

  if (auto persistence_config = config["persistence"]) {
	if (!persistence_config["persistent_file"] || !persistence_config["persist_threshold"]
		|| !persistence_config["snapshot_file"])
	  throw std::runtime_error("config file not formatted correctly.");

	util::GetLogger()->log("persistence enabled");
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

void KeyValueService::run() {
  grpc_server_ = grpc_server_builder_.BuildAndStart();
  while (!raft_server_->run());
}
bool KeyValueService::update() {
  if (raft_settings_->persistence_settings.has_value()) {
	auto recent_persist = raft_settings_->persistence_settings->recent_persists.Pop();
	bool needs_snapshot = false;
	while (recent_persist.has_value()) {
	  util::GetLogger()->log("recent persist %u < %u",
							 raft_settings_->persistence_settings->persistence_threshold,
							 recent_persist.value());
	  needs_snapshot |= raft_settings_->persistence_settings->persistence_threshold < recent_persist.value();
	  recent_persist = raft_settings_->persistence_settings->recent_persists.Pop();
	}

	if (needs_snapshot)
	  UpdateSnapshot();
  }
  return true;
}
void KeyValueService::kill() {}

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
	  auto &key = operation.get().args().key();
	  if (key.empty() || !key_value_state_.data().contains(key)) {
		operation.mutable_status()->set_code(protos::kv::KeyNotFound);
		operation.mutable_status()->set_info(key + " not found");
		break;
	  }
	  operation.mutable_get()->mutable_reply()->set_value(key_value_state_.data().at(key));
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
  }

  {
	std::unique_lock lock(*lock_);
	if (ongoing_transactions_.contains(entry.index())) {
	  auto operation_result = ongoing_transactions_[entry.index()];
	  operation_result->set_value(operation);
	  ongoing_transactions_.erase(entry.index());
	}
	last_included_index_ = entry.index();
  }
}

bool KeyValueService::UpdateSnapshot() {
  util::GetLogger()->msg(util::LogLevel::DEBUG, "Starting snapshot");
  if (!raft_settings_->persistence_settings.has_value())
	throw std::runtime_error("can't update snapshot when snapshots are not enabled");

  auto temp_file_name = "~" + raft_settings_->persistence_settings->snapshot_file;
  // TODO: use lock
  if (std::filesystem::exists(temp_file_name))
	return false;
  util::GetLogger()->msg(util::LogLevel::DEBUG, "No previous snapshot");

  //TODO: this file stuff should probably be moved into raft since InstallSnapshot is in raft and does the same things.
  std::ofstream temp_file(temp_file_name);
  key_value_state_.SerializeToOstream(&temp_file);
  temp_file.close();

  util::GetLogger()->msg(util::LogLevel::DEBUG, "here1");

  std::ifstream temp_file_read(temp_file_name);
  std::ofstream real_file(raft_settings_->persistence_settings->snapshot_file);
  real_file << temp_file_read.rdbuf();
  std::remove(temp_file_name.c_str());

  util::GetLogger()->msg(util::LogLevel::DEBUG, "here2");

  util::GetLogger()->log("starting a new snapshot");
  return raft_server_->snapshot(last_included_index_);
}
}// namespace flashpoint::keyvalue