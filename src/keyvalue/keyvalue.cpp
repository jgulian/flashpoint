#include "keyvalue/keyvalue.hpp"

namespace flashpoint::keyvalue {

bool Plugin::start(Operation &operation) {
  return start_fn_(operation);
}

void Plugin::addStart(std::function<bool(Operation &)> start) {
  start_fn_ = std::move(start);
}

bool KeyValueService::put(const std::string &key, const std::string &value) {
  auto op = Operation{PUT, key + '\0' + value};
  return start(op);
}

bool KeyValueService::get(const std::string &key, std::string &value) {
  auto op = Operation{GET, key};
  auto ok = start(op);
  if (ok)
    value = op.result;
  return ok;
}

bool KeyValueService::start(Operation &operation) {
  for (auto &plugin : plugins_)
    if (!plugin->forward(operation))
      return false;
  return storage_->doOperation(operation);
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
    plugin->addStart([service](Operation &operation) -> bool { return service->start(operation); });
    plugins.emplace_back(plugin);
  }
  service->setPlugins(plugins);

  return service;
}
}