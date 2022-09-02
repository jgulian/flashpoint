#include "util/logger.hpp"


namespace flashpoint::util {

void SimpleLogger::msg(LogLevel log_level, const std::string &message) {
  if (log_level == LogLevel::ALL) throw std::runtime_error("invalid log level all to log a message");

  if (log_level < log_level_) return;

  std::string log_message;

  if (supports_colored_text_) {
    switch (log_level) {
      case LogLevel::FATAL: log_message = "\033[1;31m[FATAL]   \033[0m" + message + "\n"; break;
      case LogLevel::ERROR2: log_message = "\033[31m[ERROR]   \033[0m" + message + "\n"; break;
      case LogLevel::WARN: log_message = "\033[33m[WARN]   \033[0m" + message + "\n"; break;
      case LogLevel::INFO: log_message = "\033[32m[INFO]   \033[0m" + message + "\n"; break;
      case LogLevel::DEBUG: log_message = "\033[1;32m[DEBUT]   \033[0m" + message + "\n"; break;
      case LogLevel::ALL: break;
    }
  } else {
    switch (log_level) {
      case LogLevel::FATAL: log_message = "[FATAL] " + message + "\n"; break;
      case LogLevel::ERROR2: log_message = "[ERROR] " + message + "\n"; break;
      case LogLevel::WARN: log_message = "[WARN] " + message + "\n"; break;
      case LogLevel::INFO: log_message = "[INFO] " + message + "\n"; break;
      case LogLevel::DEBUG: log_message = "[DEBUG] + " + message + "\n"; break;
      case LogLevel::ALL: break;
    }
  }
}

bool SimpleLogger::worker() { return false; }

void ManualLogger::msg(LogLevel log_level, const std::string &message) {
  std::lock_guard<std::mutex> lock(lock_);
  queue_.push(message);
  size++;
}

bool ManualLogger::worker() {
  if (ManualLogger::size < log_size_limit) return false;

  std::lock_guard<std::mutex> lock(ManualLogger::lock_);
  while (!ManualLogger::queue_.empty()) {
    std::cout << ManualLogger::queue_.front();
    ManualLogger::queue_.pop();
    ManualLogger::size--;
  }

  return true;
}

}// namespace flashpoint::util
