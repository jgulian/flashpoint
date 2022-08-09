#include "cmd/cmd.hpp"

namespace flashpoint::cmd {

inline void dataOrFileArgsSetup(CLI::Option_group &group, DataOrFileArgs &args, const std::string &option_name) {
  group.add_option("--" + option_name, args.data, option_name);
  group.add_option("--" + option_name + "-file", args.file, option_name + " file");
  group.require_option(1);
}

void loadDataOrFileArgs(const DataOrFileArgs &args, std::string &data) {
  if (args.data.empty()) {
    data = args.data;
  } else {
    std::ifstream file;
    file.open(args.file);
    file >> data;
    file.close();
  }
}

CLI::App *setupGetSubcommand(CLI::App &app, GetCommandArgs &command_args) {
  CLI::App *get = app.add_subcommand("get");

  get->add_option("-a,--address", command_args.host_address, "host address")->default_str(command_args.host_address);

  CLI::Option_group *key_group = get->add_option_group("key");
  dataOrFileArgsSetup(*key_group, command_args.key, "key");

  return get;
}
CLI::App *setupPutSubcommand(CLI::App &app, PutCommandArgs &command_args) {
  CLI::App *put = app.add_subcommand("put");

  put->add_option("-a,--address", command_args.host_address, "host address")->default_str(command_args.host_address);

  CLI::Option_group *key_group = put->add_option_group("key");
  dataOrFileArgsSetup(*key_group, command_args.key, "key");
  CLI::Option_group *value_group = put->add_option_group("value");
  dataOrFileArgsSetup(*value_group, command_args.value, "value");

  return put;
}
CLI::App *setupStartSubcommand(CLI::App &app, StartCommandArgs &command_args) {
  CLI::App *start = app.add_subcommand("start");

  start->add_option("-a,--address", command_args.host_address, "host address")->default_str(command_args.host_address);
  start->add_option("-s,--snapshot", command_args.server_config.snapshot_file, "file to use to store snapshots");

  return start;
}
CLI::App *setupConnectSubcommand(CLI::App &app, ConnectCommandArgs &command_args) {
  CLI::App *connect = app.add_subcommand("connect");

  connect->add_option("-a,--address", command_args.host_address, "host address")->default_str(command_args.host_address);
  connect->add_option("-p,--peer", command_args.peer_address, "peer address")->required();

  connect->add_option("-s,--snapshot", command_args.server_config.snapshot_file, "file to use to store snapshots");

  return connect;
}

void getCmd(CLI::App &get, const GetCommandArgs &command_args) {
  auto channel = grpc::CreateChannel(command_args.host_address, grpc::InsecureChannelCredentials());
  auto kv_stub = protos::kv::KeyValueApi::NewStub(channel);

  std::string key = {};
  loadDataOrFileArgs(command_args.key, key);

  grpc::ClientContext grpc_context = {};
  protos::kv::GetArgs args = {};
  args.set_key(key);
  protos::kv::GetReply reply = {};

  auto status = kv_stub->Get(&grpc_context, args, &reply);

  if (status.ok() && reply.status().code() == protos::kv::Ok) {
    std::cout << "successful" << std::endl;
    if (command_args.output_file.empty()) {
      std::cout << "key: " << args.key() << std::endl
                << "value: " << reply.value() << std::endl;
    } else {
      std::ofstream file = {};
      file.open(command_args.output_file);
      file << reply.value();
      file.close();
    }
  } else if (!status.ok()) {
    std::cout << "connection failed" << std::endl;
  } else {
    std::cout << "get request failed: " << reply.status().info() << std::endl;
  }
}
void putCmd(CLI::App &put, const PutCommandArgs &command_args) {
  auto channel = grpc::CreateChannel(command_args.host_address, grpc::InsecureChannelCredentials());
  auto kv_stub = protos::kv::KeyValueApi::NewStub(channel);

  std::string key = {}, value = {};
  loadDataOrFileArgs(command_args.key, key);
  loadDataOrFileArgs(command_args.value, value);

  grpc::ClientContext grpc_context = {};
  protos::kv::PutArgs args = {};
  args.set_key(key);
  args.set_value(value);
  protos::kv::PutReply reply = {};

  auto status = kv_stub->Put(&grpc_context, args, &reply);

  if (status.ok() && reply.status().code() == protos::kv::Ok) {
    std::cout << "successful" << std::endl;
  } else if (!status.ok()) {
    std::cout << "connection failed" << std::endl;
  } else {
    std::cout << "get request failed: " << reply.status().info() << std::endl;
  }
}
void startCmd(CLI::App &start, const StartCommandArgs &command_args) {
  keyvalue::KeyValueStorageBuilder builder = {};
  builder.addStorage(std::make_shared<keyvalue::SimpleStorage>());

  auto kv_service = builder.build();
  KeyValueAPI api_service = KeyValueAPI(*kv_service);

  grpc::ServerBuilder grpc_server_builder;
  grpc_server_builder.AddListeningPort(command_args.host_address, grpc::InsecureServerCredentials());
  grpc_server_builder.RegisterService(&api_service);
  std::unique_ptr<grpc::Server> grpc_api_server(grpc_server_builder.BuildAndStart());

  std::cout << "Started api on " << command_args.host_address << std::endl;
  grpc_api_server->Wait();
}
void connectCmd(CLI::App &connect, const ConnectCommandArgs &command_args) {
  keyvalue::KeyValueStorageBuilder builder = {};
  builder.addStorage(std::make_shared<keyvalue::SimpleStorage>());

  //auto server = PublicKeyValueApiServer(std::move(std::reinterpret_pointer_cast<Engine>(engine)));

  std::cout << "Serving started..." << std::endl;

  //server.Block();
}

}// namespace flashpoint::cmd