#include "cmd/server.hpp"

namespace flashpoint::cmd {

KeyValueAPI::KeyValueAPI(keyvalue::KeyValueService &service) : service_(service) {}

grpc::Status KeyValueAPI::Get(grpc::ServerContext *context, const protos::kv::GetArgs *request, keyvalue::Operation *response) {
  auto operation = service_.get(request->key());
  response->CopyFrom(operation);
  return grpc::Status::OK;
}
grpc::Status KeyValueAPI::Put(grpc::ServerContext *context, const protos::kv::PutArgs *request, keyvalue::Operation *response) {
  response->mutable_put()->mutable_args()->CopyFrom(*request);
  service_.start(*response);

  return grpc::Status::OK;
}

}// namespace flashpoint::cmd