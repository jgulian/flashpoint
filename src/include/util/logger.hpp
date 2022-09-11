#ifndef FLASHPOINT_SRC_INCLUDE_UTIL_LOGGER_HPP_
#define FLASHPOINT_SRC_INCLUDE_UTIL_LOGGER_HPP_

#include <atomic>
#include <iostream>
#include <memory>
#include <optional>
#include <queue>
#include <string>
#include <thread>

#include "util/concurrent_queue.hpp"

namespace flashpoint::util {

enum class LogLevel {
  FATAL = 5,
  ERR = 4,
  WARN = 3,
  INFO = 2,
  DEBUG = 1,
  ALL = 0,
};

class Logger {
 public:
  explicit Logger() : log_level_(LogLevel::ALL) {}
  virtual ~Logger() = default;

  virtual void Msg(LogLevel log_level, const std::string &message) = 0;
  virtual bool Worker() = 0;

  template<typename... Args>
  void Msg(LogLevel log_level, const std::string &format, Args... args) {
	int size_s = std::snprintf(nullptr, 0, format.c_str(), args...) + 1;
	if (size_s <= 0)
	  throw std::runtime_error("Error during formatting.");
	auto size = static_cast<size_t>(size_s);

	std::unique_ptr<char[]> buf(new char[size]);
	std::snprintf(buf.get(), size, format.c_str(), args...);
	Msg(log_level, std::string(buf.get(), buf.get() + size - 1));
  }

  void Err(const std::string &message) {
	Msg(LogLevel::ERR, message);
  }

  template<typename... Args>
  void Err(const std::string &format, Args... args) {
	Msg(LogLevel::ERR, format, args...);
  }

  void Warn(const std::string &message) {
	Msg(LogLevel::WARN, message);
  }

  template<typename... Args>
  void Warn(const std::string &format, Args... args) {
	Msg(LogLevel::WARN, format, args...);
  }

  void Log(const std::string &message) {
	Msg(LogLevel::INFO, message);
  }

  template<typename... Args>
  void Log(const std::string &format, Args... args) {
	Msg(LogLevel::INFO, format, args...);
  }

  void Debug(const std::string &message) {
	Msg(LogLevel::DEBUG, message);
  }

  template<typename... Args>
  void Debug(const std::string &format, Args... args) {
	Msg(LogLevel::DEBUG, format, args...);
  }

 protected:
  LogLevel log_level_;
};

class SimpleLogger : public Logger {
 private:
  bool supports_colored_text_ = false;
  ConcurrentQueue<std::string> queue_;
  std::thread thread_;
  std::atomic<bool> running_ = false;

 public:
  void Msg(LogLevel log_level, const std::string &message) override;
  bool Worker() override;
  bool Kill();
};

void SetLogger(std::shared_ptr<Logger> logger);
std::shared_ptr<Logger> GetLogger();

}// namespace flashpoint::util

#endif//FLASHPOINT_SRC_INCLUDE_UTIL_LOGGER_HPP_
