#ifndef FLASHPOINTDB_STORAGE_LOGGER_H_
#define FLASHPOINTDB_STORAGE_LOGGER_H_

#include <fstream>

#include <protos/api.pb.h>
#include <protos/log.pb.h>

#include "storage/engine.h"

namespace flashpoint {

class LoggerEngine : public Engine {
 public:
  LoggerEngine(std::shared_ptr<Engine>, std::string, std::string);

  std::optional<const Value> get(std::string &string) override;
  void put(std::string &string, Value value) override;

  void serialize(std::ostream &o) const override;
  void deserialize(std::istream &i) override;

  std::string getSnapshotPath() const;

 private:
  uint log_index_, commit_index_;
  std::mutex latch_;
  std::shared_ptr<Engine> sub_engine_;
  std::string storage_path_, log_path_;
};

}

#endif //FLASHPOINTDB_STORAGE_LOGGER_H_
