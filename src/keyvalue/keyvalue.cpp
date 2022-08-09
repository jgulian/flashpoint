#include "keyvalue/keyvalue.hpp"

namespace flashpoint::keyvalue {

void Plugin::start(Operation &operation) {
  start_fn_(operation);
}

void Plugin::addStart(std::function<void(Operation &)> start) {
  start_fn_ = std::move(start);
}

Operation KeyValueService::put(const std::string &key, const std::string &value) {
  auto op = Operation();
  op.mutable_put()->mutable_args()->set_key(key);
  op.mutable_put()->mutable_args()->set_value(value);

  start(op);
  return std::move(op);
}

Operation KeyValueService::get(const std::string &key) {
  auto op = Operation();
  op.mutable_get()->mutable_args()->set_key(key);

  start(op);
  if (op.status().code() == protos::kv::Code::Ok)
    op.get().reply().value();
  return std::move(op);
}

void KeyValueService::start(Operation &operation) {
  operation.mutable_status()->set_code(protos::kv::Code::Ok);
  for (auto &plugin : plugins_)
    if (!plugin->forward(operation)) {
      if (operation.status().code() == protos::kv::Code::Ok)
        operation.mutable_status()->set_code(protos::kv::Code::Error);
      return;
    }
  storage_->doOperation(operation);
}

void KeyValueService::setPlugins(std::list<std::shared_ptr<Plugin>> plugins) {
  plugins_ = std::move(plugins);
}

KeyValueStorageBuilder *KeyValueStorageBuilder::addStorage(std::shared_ptr<Storage> storage) {
  if (storage_.has_value())
    throw std::runtime_error("key value service already has storage");

  storage_ = std::move(storage);
  return this;
}

KeyValueStorageBuilder *KeyValueStorageBuilder::addPlugin(std::shared_ptr<Plugin> plugin) {
  plugins_.emplace_back(std::move(plugin));
  return this;
}

std::shared_ptr<KeyValueService> KeyValueStorageBuilder::build() {
  if (!storage_.has_value())
    throw std::runtime_error("key value must have storage");

  auto service = std::make_shared<KeyValueService>(storage_.value());

  std::list<std::shared_ptr<Plugin>> plugins = {};
  for (auto &plugin : plugins_) {
    plugin->addStart([service](Operation &operation) { service->start(operation); });
    plugins.emplace_back(plugin);
  }
  service->setPlugins(plugins);

  return service;
}
}