#include "cmd/cmd.hpp"

namespace flashpoint::cmd {

void dataOrFileArgsSetup(CLI::Option_group &group, DataOrFileArgs args, const std::string &option_name) {
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

void getCmd(CLI::App &get, const GetCommandArgs &args) {
  auto channel = grpc::CreateChannel(args.host_address, grpc::InsecureChannelCredentials());
  auto kv_stub = protos::kv::KeyValueApi::NewStub(channel);

  grpc::ClientContext grpc_context = {};
  protos::kv::GetArgs grpc_args = {};
  loadDataOrFileArgs(args.key, *grpc_args.mutable_key());
  protos::kv::GetReply grpc_reply = {};

  auto ok = true;
  grpc::Status status = {};
  try {
    status = kv_stub->Get(&grpc_context, grpc_args, &grpc_reply);
  } catch (...) {
    ok = false;
  }


  std::cout << "here" << std::endl;
  if (ok && status.ok() && grpc_reply.status().code() == protos::kv::Ok) {
    std::cout << "key: " << grpc_args.key() << std::endl;
    if (!args.key.data.empty()) {
      std::cout << grpc_reply.value();
    } else {
      std::ofstream file = {};
      file.open(args.output_file);
      file << grpc_reply.value();
      file.close();
    }
  } else if (!status.ok()) {
    std::cout << "connection failed" << std::endl;
  } else {
    std::cout << "get request failed: " << grpc_reply.status().info() << std::endl;
  }
}
void putCmd(CLI::App &put, const PutCommandArgs &args) {
}
void startCmd(CLI::App &start, const StartCommandArgs &args) {
  keyvalue::KeyValueStorageBuilder builder = {};
  builder.addStorage(std::make_shared<keyvalue::SimpleStorage>());
  //builder.addPlugin(std::make_shared<keyvalue::GrpcPlugin>(args.server_config.peer_server_address, grpc::InsecureServerCredentials()));

  //auto server = PublicKeyValueApiServer(std::move(std::reinterpret_pointer_cast<Engine>(engine)));

  std::cout << "Serving started..." << std::endl;

  auto kv_service = builder.build();
  KeyValueAPI api_service = KeyValueAPI(*kv_service);

  grpc::ServerBuilder grpc_server_builder;
  grpc_server_builder.AddListeningPort(args.host_address, grpc::InsecureServerCredentials());
  grpc_server_builder.RegisterService(&api_service);
  std::unique_ptr<grpc::Server> grpc_api_server = grpc_server_builder.BuildAndStart();

  std::cout << "Started api on " << args.host_address << std::endl;
  grpc_api_server->Wait();
}
void connectCmd(CLI::App &connect, const ConnectCommandArgs &args) {
  keyvalue::KeyValueStorageBuilder builder = {};
  builder.addStorage(std::make_shared<keyvalue::SimpleStorage>());

  //auto server = PublicKeyValueApiServer(std::move(std::reinterpret_pointer_cast<Engine>(engine)));

  std::cout << "Serving started..." << std::endl;

  //server.Block();
}

}// namespace flashpoint::cmd