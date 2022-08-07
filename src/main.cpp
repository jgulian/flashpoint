#include "keyvalue/keyvalue.hpp"
#include "keyvalue/plugins/grpc.hpp"
#include "keyvalue/storage/simple_storage.hpp"
#include "raft/grpc.hpp"

using namespace flashpoint;

int main() {
  keyvalue::KeyValueStorageBuilder builder = {};
  builder.addStorage(std::make_shared<keyvalue::SimpleStorage>());
  builder.addPlugin(std::make_shared<keyvalue::GrpcPlugin>("0.0.0.0:8080", grpc::InsecureServerCredentials()));

  //auto server = PublicKeyValueApiServer(std::move(std::reinterpret_pointer_cast<Engine>(engine)));

  std::cout << "Serving started..." << std::endl;

  //server.Block();

  return 0;
}