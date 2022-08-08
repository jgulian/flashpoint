#include "cmd/server.hpp"

namespace flashpoint::cmd {

KeyValueAPI::KeyValueAPI(keyvalue::KeyValueService &service) : service_(service) {}

grpc::Status KeyValueAPI::Get(grpc::ServerContext *context, const protos::kv::GetArgs *request, protos::kv::GetReply *response) {
  bool ok = service_.get(request->key(), *response->mutable_value());
  response->mutable_status()->set_code(ok ? protos::kv::Code::Ok : protos::kv::Code::Error);

  return grpc::Status::OK;
}
grpc::Status KeyValueAPI::Put(grpc::ServerContext *context, const protos::kv::PutArgs *request, protos::kv::PutReply *response) {
  bool ok = service_.put(request->key(), request->value());
  response->mutable_status()->set_code(ok ? protos::kv::Code::Ok : protos::kv::Code::Error);

  return grpc::Status::OK;
}

}// namespace flashpoint::cmd