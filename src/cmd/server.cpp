#include "cmd/server.hpp"

namespace flashpoint::cmd {

KeyValueAPI::KeyValueAPI(keyvalue::KeyValueService &service) : service_(service) {}

grpc::Status KeyValueAPI::Get(grpc::ServerContext *context, const protos::kv::GetArgs *request, protos::kv::GetReply *response) {
  //std::string value = {};
  //bool ok = service_.get(request->key(),value);
  //response->set_value(value);
  //response->mutable_status()->set_code(ok ? protos::kv::Code::Ok : protos::kv::Code::Error);
  //response->mutable_status()->set_info("none");
  //if (!ok)
  //  response->set_value("");
  //
  //std::cout << "get here "  << ok << " " << response->value() << std::endl;
  response->set_value(request->key());
  std::cout << "get " + request->key() << std::endl;
  return grpc::Status::OK;
}
grpc::Status KeyValueAPI::Put(grpc::ServerContext *context, const protos::kv::PutArgs *request, protos::kv::PutReply *response) {
  bool ok = service_.put(request->key(), request->value());
  response->mutable_status()->set_code(ok ? protos::kv::Code::Ok : protos::kv::Code::Error);

  std::cout << "put here" << std::endl;
  return grpc::Status::OK;
}

}// namespace flashpoint::cmd