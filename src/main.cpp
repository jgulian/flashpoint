#include <CLI/App.hpp>
#include <CLI/Formatter.hpp>
#include <CLI/Config.hpp>

#include "keyvalue/keyvalue.hpp"
#include "keyvalue/plugins/grpc.hpp"
#include "keyvalue/storage/simple_storage.hpp"
#include "raft/grpc.hpp"

using namespace flashpoint;

int start() {
  keyvalue::KeyValueStorageBuilder builder = {};
  builder.addStorage(std::make_shared<keyvalue::SimpleStorage>());
  builder.addPlugin(std::make_shared<keyvalue::GrpcPlugin>("0.0.0.0:8080", grpc::InsecureServerCredentials()));

  //auto server = PublicKeyValueApiServer(std::move(std::reinterpret_pointer_cast<Engine>(engine)));

  std::cout << "Serving started..." << std::endl;

  //server.Block();

  return 0;
}

int main(int argc, char **argv) {
  CLI::App app{"Flashpoint key/value db"};

  std::string host_address = "0.0.0.0";
  app.add_option("-a,--address", host_address, "host address");
  unsigned short port = 3308;
  app.add_option("-p,--port", port, "port");

  CLI::App *get = app.add_subcommand("get");
  CLI::App *put = app.add_subcommand("put");
  CLI::App *start = app.add_subcommand("start");
  CLI::App *connect = app.add_subcommand("connect");

  CLI11_PARSE(app, argc, argv);

  if (*get) {
    std::cout << "get" << std::endl;
  } else if (*put) {
    std::cout << "put" << std::endl;

  } else if (*start) {
    std::cout << "start" << std::endl;

  } else if (*connect) {
    std::cout << "connect" << std::endl;

  } else {
    std::cout << app.help() << std::endl;
  }

  return 0;
}