#include "keyvalue/grpc.hpp"
#include "raft/grpc.hpp"

using namespace flashpoint;

int main() {

  std::string database_file = "flashpoint.db";
  std::string database_log = "flashpoint.log";

  //auto server = PublicKeyValueApiServer(std::move(std::reinterpret_pointer_cast<Engine>(engine)));

  std::cout << "Serving started..." << std::endl;

  grpc::ServerBuilder builder;
  builder.AddListeningPort("0.0.0.0:8080", grpc::InsecureServerCredentials());

  //auto raft = raft::GrpcRaft([](const std::string &data) {
  //  std::cout << data;
  //});

  auto server = builder.BuildAndStart();
  server->Wait();

  //server.Block();

  return 0;
}