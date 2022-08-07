#ifndef FLASHPOINT_GRPC_HPP
#define FLASHPOINT_GRPC_HPP

#include <grpc++/channel.h>
#include <grpc++/server_builder.h>

#include <protos/api.grpc.pb.h>

#include "keyvalue/keyvalue.hpp"

namespace flashpoint::keyvalue {

class GrpcPlugin : public Plugin {
 private:
  class GrpcPluginService final : public protos::KeyValueApi::Service {
   public:
    explicit GrpcPluginService(GrpcPlugin &plugin);

    grpc::Status Get(::grpc::ServerContext *context, const ::protos::GetArgs *request,
                     ::protos::GetReply *response) override;

    grpc::Status Put(::grpc::ServerContext *context, const ::protos::PutArgs *request,
                     ::protos::PutReply *response) override;

   private:
    GrpcPlugin &plugin_;
  };

 public:
  GrpcPlugin(const std::string &addr_uri,
             std::shared_ptr<grpc::ServerCredentials> creds,
             int *selected_port = nullptr);

  bool forward(Operation &operation) override;

 private:
  std::unique_ptr<GrpcPluginService> service_;
  std::unique_ptr<grpc::Server> server_;
};

}// namespace flashpoint::keyvalue

#endif//FLASHPOINT_GRPC_HPP
