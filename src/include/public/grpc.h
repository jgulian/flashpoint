#ifndef FLASHPOINTDB_PUBLIC_GRPC_H_
#define FLASHPOINTDB_PUBLIC_GRPC_H_

#include <json/json.h>

#include <grpc/grpc.h>
#include <grpcpp/server_builder.h>

#include <protos/api.grpc.pb.h>

#include "storage/engine.h"

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

#endif //FLASHPOINTDB_PUBLIC_GRPC_H_
