#ifndef FLASHPOINT_RAFT_PEER_H_
#define FLASHPOINT_RAFT_PEER_H_

#include <protos/raft.grpc.pb.h>

#include "state.hpp"

namespace flashpoint::raft {

using namespace protos::raft;

class Peer {
public:
  using UpdateRequest =
      std::variant<AppendEntriesRequest, InstallSnapshotRequest>;
  using TimeOrTerm = std::variant<Time, LogTerm>;
  using Entries = google::protobuf::RepeatedPtrField<LogEntry>;

  explicit Peer(std::string client_address);

  bool requestVote(const RequestVoteRequest &request,
                   RequestVoteResponse &response);
  static void getRequestVoteRequest(const State &state,
                                    RequestVoteRequest &request);

  bool update(State &state);


  std::mutex latch_;

private:
  void getAppendEntriesRequest(const State &state,
                               AppendEntriesRequest &request) const;
  void getInstallSnapshotRequest(const State &state,
                                 InstallSnapshotRequest &request);

  void fillEntries(const State &state, Entries *entries) const;

  std::unique_ptr<protos::raft::Raft::Stub> stub_;
  Random random_;

  LogIndex next_index_, match_index_;
  std::optional<int> snapshot_offset_;
};

} // namespace flashpoint::raft

#endif // FLASHPOINT_RAFT_PEER_H_
