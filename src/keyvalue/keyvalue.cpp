#include "keyvalue/keyvalue.hpp"

namespace flashpoint::keyvalue {


KeyValueServer::KeyValueServer(KeyValueService &service) : service_(service) {}
grpc::Status KeyValueServer::Get(::grpc::ServerContext *context, const ::protos::kv::GetArgs *request,
                                 ::protos::kv::Operation *response) {
  response->CopyFrom(service_.get(request->key()));
  return grpc::Status::OK;
}
grpc::Status KeyValueServer::Put(::grpc::ServerContext *context, const ::protos::kv::PutArgs *request,
                                 ::protos::kv::Operation *response) {
  response->CopyFrom(service_.put(request->key(), request->value()));
  return grpc::Status::OK;
}

KeyValueService::KeyValueService(const std::string &address, const std::string &raft_address) : client_(nullptr) {}
Operation KeyValueService::put(const std::string &key, const std::string &value) {
  auto op = Operation();
  op.mutable_put()->mutable_args()->set_key(key);
  op.mutable_put()->mutable_args()->set_value(value);

  start(op);
  return std::move(op);
}

Operation KeyValueService::get(const std::string &key) {
  auto op = Operation();
  op.mutable_get()->mutable_args()->set_key(key);

  start(op);
  if (op.status().code() == protos::kv::Code::Ok) op.get().reply().value();
  return std::move(op);
}

void KeyValueService::start(Operation &operation) {
  operation.mutable_status()->set_code(protos::kv::Code::Ok);
  throw std::runtime_error("not completed");
}

KeyValueServer::~KeyValueServer() { throw std::runtime_error("not completed"); }
}// namespace flashpoint::keyvalue