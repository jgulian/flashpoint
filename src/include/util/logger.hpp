#ifndef FLASHPOINT_SRC_INCLUDE_UTIL_LOGGER_HPP_
#define FLASHPOINT_SRC_INCLUDE_UTIL_LOGGER_HPP_

#include <iostream>
#include <memory>
#include <string>

namespace flashpoint::util {

enum class LogLevel {
  FATAL = 5,
  ERROR2 = 4,
  WARN = 3,
  INFO = 2,
  DEBUG = 1,
  ALL = 0,
};

class Logger {
 public:
  explicit Logger(const LogLevel &log_level) : log_level_(log_level) {}

  virtual ~Logger() = default;

  virtual void msg(LogLevel log_level, const std::string &message) = 0;

  template<typename... Args>
  void msg(LogLevel log_level, const std::string &format, Args... args) {
    int size_s = std::snprintf(nullptr, 0, format.c_str(), args...) + 1;
    if (size_s <= 0)
      throw std::runtime_error("Error during formatting.");
    auto size = static_cast<size_t>(size_s);

    std::unique_ptr<char[]> buf(new char[size]);
    std::snprintf(buf.get(), size, format.c_str(), args...);
    msg(log_level, std::string(buf.get(), buf.get() + size - 1));
  }

  void log(const std::string &message) {
    msg(LogLevel::INFO, message);
  }

  template<typename... Args>
  void log(const std::string &format, Args... args) {
    msg(LogLevel::INFO, format, args...);
  }

  void err(const std::string &message) {
    msg(LogLevel::ERROR2, message);
  }

  template<typename... Args>
  void err(const std::string &format, Args... args) {
    msg(LogLevel::ERROR2, format, args...);
  }

 protected:
  LogLevel log_level_;
};

class SimpleLogger : public Logger {
 public:
  explicit SimpleLogger(const LogLevel &log_level = LogLevel::INFO);

  ~SimpleLogger() override = default;

  void msg(LogLevel log_level, const std::string &message) override;

  void worker();

 private:
  bool supports_colored_text_ = true;
};

static std::unique_ptr<Logger> LOGGER;

}// namespace flashpoint::util

#endif//FLASHPOINT_SRC_INCLUDE_UTIL_LOGGER_HPP_
