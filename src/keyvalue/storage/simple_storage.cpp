#include "keyvalue/storage/simple_storage.hpp"
#include <iostream>

namespace flashpoint::keyvalue {

bool SimpleStorage::doOperation(Operation &operation) {
  for (auto &[k, v] : storage_)
    std::cout << "key: " << k << "\t value:" << v << std::endl;

  switch (operation.data_case()) {
    case protos::kv::Operation::kPut: {
      storage_.insert({operation.put().args().key(), operation.put().args().value()});
    } break;
    case protos::kv::Operation::kGet: {
      auto &key = operation.get().args().key();
      if (storage_.contains(key)) {
        operation.mutable_get()->mutable_reply()->set_value(storage_.at(key));
      } else {
        operation.mutable_status()->set_code(protos::kv::Code::KeyNotFound);
        operation.mutable_status()->set_info(key + " not found");
      }
    } break;
    case protos::kv::Operation::DATA_NOT_SET:
      operation.mutable_status()->set_code(protos::kv::Code::InvalidOperation);
  }
  return true;
}

}// namespace flashpoint::keyvalue