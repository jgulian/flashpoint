#include "storage/engines/logger_engine.hpp"

namespace flashpoint {

LoggerEngine::LoggerEngine(std::shared_ptr<Engine> sub_engine, std::string storage_path, std::string log_path)
    : sub_engine_(std::move(sub_engine)),
      storage_path_(std::move(storage_path)),
      log_path_(std::move(log_path)),
      log_index_(0) {
  std::ifstream storage_file(storage_path_.c_str());
  if (!storage_file.good()) return;

  // Attempt Recovery
  sub_engine_->deserialize(storage_file);

  // TODO: Complete
}

std::optional<const Value> LoggerEngine::get(std::string &key) {
  return sub_engine_->get(key);
}
void LoggerEngine::put(std::string &key, Value value) {
  auto log_entry = std::make_unique<protos::log::LogEntry>();
  auto log_index = log_index_++;
  log_entry->set_commit_index(log_index);

  auto sub_value = std::make_unique<protos::Value>(value.getValue());
  log_entry->mutable_put_args()->set_key(key);
  log_entry->mutable_put_args()->set_allocated_value(sub_value.release());

  {
    std::ofstream log(log_path_, std::ios::ate | std::ios::binary | std::ios::app);
    log_entry->SerializeToOstream(&log);
    log.close();
  }

  sub_engine_->put(key, value);

  std::ofstream store(storage_path_, std::ios::ate | std::ios::binary);
  store << *sub_engine_;
  store.close();

  auto commit_log_entry = std::make_unique<protos::log::LogEntry>();
  commit_log_entry->set_log_index(log_index_++);
  commit_log_entry->set_commit_index(log_index);

  {
    std::ofstream log(log_path_, std::ios::ate | std::ios::binary | std::ios::app);
    commit_log_entry->SerializeToOstream(&log);
    log.close();
  }
}
void LoggerEngine::serialize(std::ostream &o) const {
  o << storage_path_ << log_path_;
}
void LoggerEngine::deserialize(std::istream &i) {
  i >> storage_path_ >> log_path_;
}

std::string LoggerEngine::getSnapshotPath() const {
  return std::move("~" + storage_path_);
}

}
