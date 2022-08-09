#ifndef FLASHPOINT_KEYVALUE_HPP
#define FLASHPOINT_KEYVALUE_HPP

#include <protos/kv.pb.h>

#include <functional>
#include <list>
#include <memory>
#include <optional>
#include <unordered_map>

namespace flashpoint::keyvalue {
using Operation = protos::kv::Operation;
using Status = protos::kv::Status;

class KeyValueStorageBuilder;

class Plugin {
  friend KeyValueStorageBuilder;

 public:
  Plugin() = default;
  Plugin(const Plugin &plugin) = delete;

  virtual bool forward(Operation &operation) = 0;

 protected:
  void start(Operation &operation);

 private:
  void addStart(std::function<void(Operation &operation)> start);

  std::function<void(Operation &operation)> start_fn_;
};

    class Storage {
    public:
     Storage() = default;
     Storage(const Storage &storage) = delete;

     virtual bool doOperation(Operation &operation) = 0;
    };

    class KeyValueService {
      friend KeyValueStorageBuilder;

     public:
      explicit KeyValueService(std::shared_ptr<Storage> storage) : storage_(std::move(storage)) {}

      void start(Operation &operation);

      Operation put(const std::string &key, const std::string &value);
      Operation get(const std::string &key);

     private:
      void setPlugins(std::list<std::shared_ptr<Plugin>> plugins);

      std::shared_ptr<Storage> storage_;
      std::list<std::shared_ptr<Plugin>> plugins_;
    };

    class KeyValueStorageBuilder {
     public:
      KeyValueStorageBuilder *addStorage(std::shared_ptr<Storage> storage);
      KeyValueStorageBuilder *addPlugin(std::shared_ptr<Plugin> plugin);

      std::shared_ptr<KeyValueService> build();

     private:
      std::list<std::shared_ptr<Plugin>> plugins_;
      std::optional<std::shared_ptr<Storage>> storage_;
    };
}

#endif //FLASHPOINT_KEYVALUE_HPP
