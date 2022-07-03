#include "public/grpc.hpp"

#include <utility>

namespace flashpoint {

PublicKeyValueApi::PublicKeyValueApi(std::shared_ptr<Engine> engine)
    : storage_engine_(std::move(engine)) {}

::grpc::Status PublicKeyValueApi::Get(::grpc::ServerContext *context,
                                      const ::protos::GetArgs *request,
                                      ::protos::GetReply *reply) {
  auto key = request->key();
  auto value_ref = storage_engine_->get(key);

  if (!value_ref.has_value()) {
    reply->mutable_status()->set_code(protos::Code::Error);
  } else {
    auto value = new protos::Value(value_ref->getValue());
    reply->set_allocated_value(value);
    reply->mutable_status()->set_code(protos::Code::Ok);
  }

  return ::grpc::Status::OK;
}
::grpc::Status PublicKeyValueApi::Put(::grpc::ServerContext *context,
                                      const ::protos::PutArgs *request,
                                      ::protos::PutReply *reply) {
  auto key = request->key();
  auto value = Value(request->value());

  storage_engine_->put(key, value);
  std::cout << "here7" << std::endl;

  reply->mutable_status()->set_code(protos::Code::Ok);

  return ::grpc::Status::OK;
}

PublicKeyValueApiServer::PublicKeyValueApiServer(std::shared_ptr<Engine> engine)
    : service_{std::move(engine)} {
  grpc::ServerBuilder builder;
  builder.AddListeningPort("0.0.0.0:8080", grpc::InsecureServerCredentials());

  builder.RegisterService(&service_);

  server_ = builder.BuildAndStart();
}
PublicKeyValueApiServer::~PublicKeyValueApiServer() {
  server_->Shutdown();
}
void PublicKeyValueApiServer::Block() {
  server_->Wait();
}

}