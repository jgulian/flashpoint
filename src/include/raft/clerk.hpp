#ifndef FLASHPOINT_RAFT_CLERK_H_
#define FLASHPOINT_RAFT_CLERK_H_

#include "raft/raft.hpp"

namespace flashpoint::raft {

class Clerk {
public:
  virtual std::string getClerkId() = 0;

  virtual void commit(const std::string &data) = 0;
  virtual std::optional<std::string> snapshot() = 0;

protected:
  Clerk(Raft &raft);

  std::future<bool> start(const std::string &data);
};

} // namespace flashpoint::raft

#endif // FLASHPOINT_RAFT_CLERK_H_
