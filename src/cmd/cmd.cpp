#include "cmd/cmd.hpp"

namespace flashpoint::cmd {

inline void DataOrFileArgsSetup(CLI::Option_group &group, DataOrFileArgs &args, const std::string &option_name) {
  group.add_option("--" + option_name, args.data, option_name);
  group.add_option("--" + option_name + "-file", args.file, option_name + " file");
  group.require_option(1);
}

void LoadDataOrFileArgs(const DataOrFileArgs &args, std::string &data) {
  if (!args.data.empty()) {
	data = args.data;
  } else {
	std::ifstream file;
	file.open(args.file);
	file >> data;
	file.close();
  }
}

CLI::App *SetupGetSubcommand(CLI::App &app, GetCommandArgs &command_args) {
  CLI::App *get = app.add_subcommand("get");

  get->add_option("-a,--address", command_args.host_address, "host address")->default_str(command_args.host_address);

  CLI::Option_group *key_group = get->add_option_group("key");
  DataOrFileArgsSetup(*key_group, command_args.key, "key");

  return get;
}
CLI::App *SetupPutSubcommand(CLI::App &app, PutCommandArgs &command_args) {
  CLI::App *put = app.add_subcommand("put");

  put->add_option("-a,--address", command_args.host_address, "host address")->default_str(command_args.host_address);

  CLI::Option_group *key_group = put->add_option_group("key");
  DataOrFileArgsSetup(*key_group, command_args.key, "key");
  CLI::Option_group *value_group = put->add_option_group("value");
  DataOrFileArgsSetup(*value_group, command_args.value, "value");

  return put;
}
CLI::App *SetupCasSubcommand(CLI::App &app, CasCommandArgs &command_args) {
  CLI::App *cas = app.add_subcommand("cas");

  cas->add_option("-a,--address", command_args.host_address, "host address")->default_str(command_args.host_address);

  CLI::Option_group *key_group = cas->add_option_group("key");
  DataOrFileArgsSetup(*key_group, command_args.key, "key");
  CLI::Option_group *expected_group = cas->add_option_group("expected");
  DataOrFileArgsSetup(*expected_group, command_args.expected, "expected");
  CLI::Option_group *update_group = cas->add_option_group("Update");
  DataOrFileArgsSetup(*update_group, command_args.updated, "Update");

  return cas;
}
CLI::App *SetupStartSubcommand(CLI::App &app, StartCommandArgs &command_args) {
  CLI::App *start = app.add_subcommand("start");

  start->add_option("-c,--config", command_args.config_file, "config file for key-value and raft")->required();
  start->add_option("-s,--snapshot", command_args.snapshot_file, "file to use to store snapshots");

  return start;
}

void GetCmd(CLI::App &get, const GetCommandArgs &command_args) {
  auto channel = grpc::CreateChannel(command_args.host_address, grpc::InsecureChannelCredentials());
  auto kv_stub = protos::kv::KeyValueApi::NewStub(channel);

  std::string key = {};
  LoadDataOrFileArgs(command_args.key, key);

  grpc::ClientContext grpc_context = {};
  protos::kv::GetArgs args = {};
  args.set_key(key);
  protos::kv::Operation reply = {};
  auto status = kv_stub->Get(&grpc_context, args, &reply);

  if (status.ok() && reply.status().code() == protos::kv::Ok) {
    std::cout << "successful" << std::endl;
    if (command_args.output_file.empty()) {
      std::cout << "key: " << args.key() << std::endl
                << "value: " << reply.get().reply().value() << std::endl;
    } else {
      std::ofstream file = {};
	  file.open(command_args.output_file);
	  file << reply.get().reply().value();
	  file.close();
	}
  } else if (!status.ok()) {
	std::cout << "connection failed" << std::endl;
  } else {
	std::cout << "get request failed: " << reply.status().code() << " " << reply.status().info() << std::endl;
  }
}
void PutCmd(CLI::App &put, const PutCommandArgs &command_args) {
  auto channel = grpc::CreateChannel(command_args.host_address, grpc::InsecureChannelCredentials());
  auto kv_stub = protos::kv::KeyValueApi::NewStub(channel);

  std::string key{}, value{};
  LoadDataOrFileArgs(command_args.key, key);
  LoadDataOrFileArgs(command_args.value, value);

  grpc::ClientContext grpc_context{};
  protos::kv::PutArgs args{};
  args.set_key(key);
  args.set_value(value);
  protos::kv::Operation reply{};
  auto status = kv_stub->Put(&grpc_context, args, &reply);

  if (status.ok() && reply.status().code() == protos::kv::Ok) {
	std::cout << "successful" << std::endl;
  } else if (!status.ok()) {
	std::cout << "connection failed" << std::endl;
  } else {
	std::cout << "put request failed - code: " << reply.status().code() << ", reason: " << reply.status().info()
			  << std::endl;
  }
}
void CasCmd(CLI::App &cas, const CasCommandArgs &command_args) {
  auto channel = grpc::CreateChannel(command_args.host_address, grpc::InsecureChannelCredentials());
  auto kv_stub = protos::kv::KeyValueApi::NewStub(channel);

  std::string key{}, expected{}, updated{};
  LoadDataOrFileArgs(command_args.key, key);
  LoadDataOrFileArgs(command_args.expected, expected);
  LoadDataOrFileArgs(command_args.updated, updated);

  grpc::ClientContext grpc_context = {};
  protos::kv::CasArgs args = {};
  args.set_key(key);
  args.set_expected(expected);
  args.set_updated(updated);
  protos::kv::Operation reply = {};
  auto status = kv_stub->Cas(&grpc_context, args, &reply);

  if (status.ok() && reply.status().code() == protos::kv::Ok) {
	std::cout << "successful" << std::endl;
  } else if (!status.ok()) {
	std::cout << "connection failed" << std::endl;
  } else if (reply.status().code() == protos::kv::UnexpectedValue) {
	std::cout << "unexpected value: " << reply.cas().reply().actual() << std::endl;
  } else {
	std::cout << "cas request failed - code: " << reply.status().code() << ", reason: " << reply.status().info()
			  << std::endl;
  }
}
void StartCmd(CLI::App &start, const StartCommandArgs &command_args) {
  keyvalue::KeyValueService service(command_args.config_file);
  service.Run();
  util::GetLogger()->Log("starting server");
  while (service.Update());
}

}// namespace flashpoint::cmd