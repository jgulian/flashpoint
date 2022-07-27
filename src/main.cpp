#include "public/grpc.hpp"
#include "raft/grpc.hpp"
#include "storage/engines/logger_engine.hpp"
#include "storage/engines/proto_engine.hpp"

using namespace flashpoint;

int main() {

  std::string database_file = "flashpoint.db";
  std::string database_log = "flashpoint.log";

  auto base_engine = std::make_shared<ProtoEngine>();
  auto engine = std::make_shared<LoggerEngine>(base_engine, database_file, database_log);

  //auto server = PublicKeyValueApiServer(std::move(std::reinterpret_pointer_cast<Engine>(engine)));

  std::cout << "Serving started..." << std::endl;

  grpc::ServerBuilder builder;
  builder.AddListeningPort("0.0.0.0:8080", grpc::InsecureServerCredentials());

  auto raft = raft::GrpcRaft([](const std::string &data) {
    std::cout << data;
  });

  auto server = builder.BuildAndStart();
  server->Wait();

  //server.Block();

  return 0;
}