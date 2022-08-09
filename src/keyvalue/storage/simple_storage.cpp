#include "keyvalue/storage/simple_storage.hpp"
#include <iostream>

namespace flashpoint::keyvalue {

bool SimpleStorage::doOperation(Operation &operation) {
  std::cout << operation.type << " " << operation.request << std::endl;
  for (auto &[k, v] : storage_)
    std::cout << "key: " << k << "\t value:" << v << std::endl;

  switch (operation.type) {
    case PUT: {
      auto split = operation.request.find('\0');
      storage_.insert({operation.request.substr(0, split), operation.request.substr(split)});
    } break;
    case GET: {
      auto &key = operation.request;
      if (storage_.contains(key))
        operation.result = storage_.at(key);
    } break;
  }
  return true;
}

}// namespace flashpoint::keyvalue