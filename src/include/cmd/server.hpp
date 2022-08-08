#ifndef FLASHPOINT_SERVER_HPP
#define FLASHPOINT_SERVER_HPP

#include <grpcpp/create_channel.h>
#include <protos/kv.grpc.pb.h>

#include "keyvalue/keyvalue.hpp"

namespace flashpoint::cmd {

class KeyValueAPI final : public protos::kv::KeyValueApi::Service {
 public:
  explicit KeyValueAPI(keyvalue::KeyValueService &service);

  grpc::Status Get(grpc::ServerContext *context, const protos::kv::GetArgs *request, protos::kv::GetReply *response) override;
  grpc::Status Put(grpc::ServerContext *context, const protos::kv::PutArgs *request, protos::kv::PutReply *response) override;

 private:
  keyvalue::KeyValueService &service_;
};
}// namespace flashpoint::cmd

#endif//FLASHPOINT_SERVER_HPP
