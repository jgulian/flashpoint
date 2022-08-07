#include "keyvalue/plugins/grpc.hpp"

namespace flashpoint::keyvalue {

GrpcPlugin::GrpcPluginService::GrpcPluginService(GrpcPlugin &plugin) : Service(), plugin_(plugin) {}

grpc::Status GrpcPlugin::GrpcPluginService::Get(::grpc::ServerContext *context, const ::protos::GetArgs *request,
                                                ::protos::GetReply *response) {
  return Service::Get(context, request, response);
}

grpc::Status GrpcPlugin::GrpcPluginService::Put(::grpc::ServerContext *context, const ::protos::PutArgs *request,
                                                ::protos::PutReply *response) {
  return Service::Put(context, request, response);
}

bool GrpcPlugin::forward(Operation &operation) {
  return true;
}

GrpcPlugin::GrpcPlugin(const std::string &addr_uri, std::shared_ptr<grpc::ServerCredentials> creds,
                       int *selected_port) : service_(std::make_unique<GrpcPluginService>(*this)) {
  grpc::ServerBuilder builder;
  builder.AddListeningPort(addr_uri, creds, selected_port);
  builder.RegisterService(service_.get());
}

}// namespace flashpoint::keyvalue