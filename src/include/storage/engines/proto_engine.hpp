#ifndef FLASHPOINTDB_STORAGE_PROTO_ENGINE_H_
#define FLASHPOINTDB_STORAGE_PROTO_ENGINE_H_

#include <optional>

#include "storage/engine.hpp"

namespace flashpoint {

class ProtoEngine : public Engine {
 public:
  ProtoEngine();

  std::optional<const Value> get(std::string&) override;
  void put(std::string&, Value) override;

  void serialize(std::ostream &o) const override;
  void deserialize(std::istream &i) override;

 private:
  Document document_;
};

}

#endif //FLASHPOINTDB_STORAGE_PROTO_ENGINE_H_
