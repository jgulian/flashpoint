#ifndef FLASHPOINTDB_RAFT_CLERK_ENGINE_H_
#define FLASHPOINTDB_RAFT_CLERK_ENGINE_H_

#include "storage/engine.h"

 namespace flashpoint {

class RaftClerkEngine : public Engine {
 public:
  std::optional<const Value> get(std::string &string) override;
  void put(std::string &string, Value value) override;
  void serialize(std::ostream &o) const override;
  void deserialize(std::istream &i) override;
};

}

#endif //FLASHPOINTDB_RAFT_CLERK_ENGINE_H_
