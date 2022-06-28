#include "storage/engines/raft_clerk_engine.h"

namespace flashpoint {

std::optional<const Value> flashpoint::RaftClerkEngine::get(std::string &string) {
  return std::nullopt;
}
void flashpoint::RaftClerkEngine::put(std::string &string, flashpoint::Value value) {

}
void flashpoint::RaftClerkEngine::serialize(std::ostream &o) const {

}
void flashpoint::RaftClerkEngine::deserialize(std::istream &i) {

}

}
