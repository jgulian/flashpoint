
#include "storage/document.h"

#include <utility>

namespace flashpoint {

Value::Value() : value_() {}
Value::Value(protos::Value value) : value_(std::move(value)) {}

Value::Value(double value) : value_() {
  auto number = protos::Number();
  number.set_decimal(value);
  value_.set_allocated_number_value(&number);
}

const protos::Value &Value::getValue() const {
  return value_;
}
protos::Value &Value::getMutableValue() {
  return value_;
}

Document::Document() : Value() {
  value_.mutable_document_value();
}

Document::Document(protos::Document document) : Value() {
  *value_.mutable_document_value() = std::move(document);
}

void Document::put(const std::string &key, const Value &value) {
  (*value_.mutable_document_value()->mutable_document())[key] = value.getValue();
}
std::optional<Value> Document::get(const std::string &key) const {
  if (!value_.document_value().document().contains(key))
    return std::nullopt;

  return Value(value_.document_value().document().at(key));
}

}
