#ifndef FLASHPOINT_SRC_INCLUDE_RAFT_CLIENT_HPP_
#define FLASHPOINT_SRC_INCLUDE_RAFT_CLIENT_HPP_

#include "raft/raft.hpp"

namespace flashpoint::raft {

class RaftClient {
 private:
  PeerId me_;
  protos::raft::Config config_ = {};
  std::optional<std::pair<std::string, RaftConnection>> cached_connection_info_ = std::nullopt;
  std::unique_ptr<std::shared_mutex> lock_ = std::make_unique<std::shared_mutex>();

 public:
  explicit RaftClient(PeerId me, const protos::raft::Config &config);

  void UpdateConfig(const protos::raft::Config &config);

  LogIndex Start(const std::string &command);

  LogIndex StartConfig(const protos::raft::Config &config);

 private:
  LogIndex DoRequest(const protos::raft::StartRequest &request);

  void CheckForCacheExistence(std::shared_lock<std::shared_mutex> &lock);

  void UpdateCache(std::shared_lock<std::shared_mutex> &lock, const std::string &leader_id);
};

}

#endif //FLASHPOINT_SRC_INCLUDE_RAFT_CLIENT_HPP_
