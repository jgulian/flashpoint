#include "keyvalue/keyvalue.hpp"

namespace flashpoint::keyvalue {

    bool Plugin::start(Operation &operation) {
        return start_fn_(operation);
    }

    bool KeyValueService::put(const std::string &key, const std::string &value) {
        auto op = Operation{PUT, key + '\0' + value};
        return forwardOperation(op);
    }

    bool KeyValueService::get(const std::string &key, std::string &value) {
        auto op = Operation{GET, key};
        auto ok = forwardOperation(op);
        if (ok)
            value = op.request;
        return ok;
    }

    bool KeyValueService::forwardOperation(Operation &operation) {
        for (auto &plugin: plugins_)
            if (!plugin->forward(operation))
                return false;
        return storage_->doOperation(operation);
    }

    KeyValueStorageService *KeyValueStorageService::addStorage(std::shared_ptr<Storage> storage) {
        if (storage_.has_value())
            throw std::runtime_error("key value service already has storage");

        storage_ = std::move(storage);
        return this;
    }

    KeyValueStorageService *KeyValueStorageService::addPlugin(std::shared_ptr<Plugin> plugin) {
        plugins_.emplace_back(std::move(plugin));
        return this;
    }

    KeyValueService KeyValueStorageService::build() {
        if (!storage_.has_value())
            throw std::runtime_error("key value must have storage");

        return KeyValueService(storage_.value(), plugins_);
    }
}