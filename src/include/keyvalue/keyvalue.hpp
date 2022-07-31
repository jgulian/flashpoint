#ifndef FLASHPOINT_KEYVALUE_HPP
#define FLASHPOINT_KEYVALUE_HPP

#include <functional>
#include <list>
#include <memory>
#include <optional>

#include "operation.hpp"

namespace flashpoint::keyvalue {
    class Plugin {
    public:
        explicit Plugin(std::function<bool(Operation &operation)> start) : start_fn_(std::move(start)) {}
        Plugin(const Plugin& plugin) = delete;

        virtual bool forward(Operation &operation) = 0;

    protected:
        bool start(Operation &operation);

    private:
        std::function<bool(Operation &operation)> start_fn_;
    };

    class Storage {
    public:
        Storage(const Storage& storage) = delete;

        virtual bool doOperation(Operation &operation) = 0;
    };

    class KeyValueService {
    public:
        explicit KeyValueService(std::shared_ptr<Storage> storage, std::list<std::shared_ptr<Plugin>> plugins)
        : storage_(std::move(storage)), plugins_(std::move(plugins)) {}

        bool put(const std::string &key, const std::string &value);
        bool get(const std::string &key, std::string &value);

    private:
        bool forwardOperation(Operation &operation);

        std::shared_ptr<Storage> storage_;
        std::list<std::shared_ptr<Plugin>> plugins_;
    };

    class KeyValueStorageService {
    public:
        KeyValueStorageService *addStorage(std::shared_ptr<Storage> storage);
        KeyValueStorageService *addPlugin(std::shared_ptr<Plugin> plugin);

        KeyValueService build();

    private:
        std::list<std::shared_ptr<Plugin>> plugins_;
        std::optional<std::shared_ptr<Storage>> storage_;
    };
}

#endif //FLASHPOINT_KEYVALUE_HPP
