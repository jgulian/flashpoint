#ifndef FLASHPOINTDB_STORAGE_ENGINE_H_
#define FLASHPOINTDB_STORAGE_ENGINE_H_

#include <json/json.h>

#include <unordered_map>

#include "storage/document.h"

namespace flashpoint {

class Path {

};

class Engine {
 public:
  virtual std::optional<const Value> get(std::string &) = 0;
  virtual void put(std::string &, Value) = 0;

  virtual void serialize(std::ostream &o) const = 0;
  virtual void deserialize(std::istream &i) = 0;

  friend std::ostream &operator<<(std::ostream &o, const Engine &engine) {
    engine.serialize(o);
    return o;
  }

  friend std::istream &operator>>(std::istream &i, Engine &engine) {
    engine.deserialize(i);
    return i;
  }

};

}

#endif //FLASHPOINTDB_STORAGE_ENGINE_H_
