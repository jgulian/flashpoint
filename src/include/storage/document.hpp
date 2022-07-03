#ifndef FLASHPOINTDB_DOCUMENT_H_
#define FLASHPOINTDB_DOCUMENT_H_

#include <protos/storage.pb.h>

namespace flashpoint {

class Value {
 public:
  explicit Value(protos::Value value);

  explicit Value(double value);

  const protos::Value &getValue() const;
  protos::Value &getMutableValue();

 protected:
  Value();

  protos::Value value_;
};

class Document : public Value {
 public:
  Document();
  explicit Document(protos::Document);

  void put(const std::string &key, const Value &value);
  std::optional<Value> get(const std::string &) const;
};

class Collection : public Document {
 public:

};

}

#endif //FLASHPOINTDB_DOCUMENT_H_
