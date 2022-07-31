#include "keyvalue/grpc.hpp"

namespace flashpoint::keyvalue {

PublicKeyValueApi::PublicKeyValueApi(std::shared_ptr<KeyValueService> service)
    : storage_engine_(std::move(service)) {}

::grpc::Status PublicKeyValueApi::Get(::grpc::ServerContext *context,
                                      const ::protos::GetArgs *request,
                                      ::protos::GetReply *reply) {
  std::string value = {};
  storage_engine_->get(request->key(), value);

  reply->mutable_status()->set_code(protos::Code::Ok);
  reply->set_value(value);

  return ::grpc::Status::OK;
}
::grpc::Status PublicKeyValueApi::Put(::grpc::ServerContext *context,
                                      const ::protos::PutArgs *request,
                                      ::protos::PutReply *reply) {
  storage_engine_->put(request->key(), request->value());
  std::cout << "here7" << std::endl;

  reply->mutable_status()->set_code(protos::Code::Ok);

  return ::grpc::Status::OK;
}

PublicKeyValueApiServer::PublicKeyValueApiServer(std::shared_ptr<KeyValueService> service)
    : service_{std::move(service)} {
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