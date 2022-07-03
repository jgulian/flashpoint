#include "storage/engines/proto_engine.hpp"

namespace flashpoint {

ProtoEngine::ProtoEngine() : document_() {}

std::optional<const Value> ProtoEngine::get(std::string &key) {
  return document_.get(key);
}
void ProtoEngine::put(std::string &key, Value value) {
  document_.put(key, value);
}

void ProtoEngine::serialize(std::ostream &o) const {
    document_.getValue().SerializeToOstream(&o);
}
void ProtoEngine::deserialize(std::istream &i) {
  document_.getMutableValue().ParseFromIstream(&i);
}


}