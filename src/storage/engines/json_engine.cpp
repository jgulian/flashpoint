#include "storage/engines/json_engine.h"

namespace flashpoint {
/*
std::optional<const Json::Value> JsonEngine::Get(std::string &document_id, Json::Path &path) {
  if (!documents_.contains(document_id))
    return std::nullopt;

  auto &document = documents_.at(document_id);
  auto &value = path.resolve(document);

  if (value.isObject() || value.isArray())
    return std::nullopt;

  return value;
}
void JsonEngine::Put(std::string &document_id, Json::Path &path, Json::Value &value) {
  if (!documents_.contains(document_id))
    documents_[document_id] = {};

  auto &document = documents_.at(document_id);
  path.make(document) = value;
}*/

}