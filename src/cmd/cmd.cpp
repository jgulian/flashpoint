#include "cmd/cmd.hpp"

namespace flashpoint::cmd {

void dataOrFileArgsSetup(CLI::Option_group &group, DataOrFileArgs args, const std::string &option_name) {
  group.add_option("--" + option_name, args.data, option_name);
  group.add_option("--" + option_name + "-file");
  group.require_option(1);
}

std::pair<CLI::App *, GetCommandArgs> setupGetSubcommand(CLI::App &app) {
  CLI::App *get = app.add_subcommand("get");
  GetCommandArgs command_args = {};

  get->add_option("-a,--address", command_args.host_address, "host address")->default_str(command_args.host_address);

  CLI::Option_group *key_group = get->add_option_group("key");
  dataOrFileArgsSetup(*key_group, command_args.key, "key");

  return {get, command_args};
}
std::pair<CLI::App *, PutCommandArgs> setupPutSubcommand(CLI::App &app) {
  CLI::App *put = app.add_subcommand("put");
  PutCommandArgs command_args = {};

  put->add_option("-a,--address", command_args.host_address, "host address")->default_str(command_args.host_address);

  CLI::Option_group *key_group = put->add_option_group("key");
  dataOrFileArgsSetup(*key_group, command_args.key, "key");
  CLI::Option_group *value_group = put->add_option_group("value");
  dataOrFileArgsSetup(*value_group, command_args.value, "value");

  return {put, command_args};
}
std::pair<CLI::App *, StartCommandArgs> setupStartSubcommand(CLI::App &app) {
  CLI::App *start = app.add_subcommand("start");
  StartCommandArgs command_args = {};

  start->add_option("-a,--address", command_args.host_address, "host address")->default_str(command_args.host_address);

  start->add_option("-s,--snapshot", command_args.server_config.snapshot_file, "file to use to store snapshots");

  return {start, command_args};
}
std::pair<CLI::App *, ConnectCommandArgs> setupConnectSubcommand(CLI::App &app) {
  CLI::App *connect = app.add_subcommand("connect");
  ConnectCommandArgs command_args;

  connect->add_option("-a,--address", command_args.host_address, "host address")->default_str(command_args.host_address);
  connect->add_option("-p,--peer", command_args.peer_address, "peer address")->required();

  connect->add_option("-s,--snapshot", command_args.server_config.snapshot_file, "file to use to store snapshots");

  return {connect, command_args};
}

void getCmd(const GetCommandArgs &args) {
}
void putCmd(const PutCommandArgs &args) {
}
void startCmd(const StartCommandArgs &args) {
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
void connectCmd(const ConnectCommandArgs &args) {
  keyvalue::KeyValueStorageBuilder builder = {};
  builder.addStorage(std::make_shared<keyvalue::SimpleStorage>());

  //auto server = PublicKeyValueApiServer(std::move(std::reinterpret_pointer_cast<Engine>(engine)));

  std::cout << "Serving started..." << std::endl;

  //server.Block();
}

}// namespace flashpoint::cmd