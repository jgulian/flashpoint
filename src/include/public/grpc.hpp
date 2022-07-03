#ifndef FLASHPOINT_SRC_INCLUDE_PUBLIC_GRPC_HPP
#define FLASHPOINT_SRC_INCLUDE_PUBLIC_GRPC_HPP

#include <grpc/grpc.h>
#include <grpcpp/server_builder.h>

#include <protos/api.grpc.pb.h>

#include "storage/engine.hpp"

namespace flashpoint {

class PublicKeyValueApi final : public protos::KeyValueApi::Service {
 public:
  explicit PublicKeyValueApi(std::shared_ptr<Engine>);

  ::grpc::Status Get(::grpc::ServerContext *context,
                     const ::protos::GetArgs *request,
                     ::protos::GetReply *reply) override;
  ::grpc::Status Put(::grpc::ServerContext *context,
                     const ::protos::PutArgs *request,
                     ::protos::PutReply *reply) override;

 private:
  std::shared_ptr<Engine> storage_engine_;
};

class PublicKeyValueApiServer {
 public:
  explicit PublicKeyValueApiServer(std::shared_ptr<Engine>);
  ~PublicKeyValueApiServer();

  void Block();

 private:
  PublicKeyValueApi service_;
  std::unique_ptr<grpc::Server> server_;
};

}

#endif // FLASHPOINT_SRC_INCLUDE_PUBLIC_GRPC_HPP
