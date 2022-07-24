#include "util/logger.hpp"

namespace flashpoint::util {

SimpleLogger::SimpleLogger(const LogLevel &log_level) : Logger(log_level) {
  thread_ = std::thread([this]() {
    while (true) {
      try {
        auto message = message_channel_.read();
        std::cout << message;
      } catch (const std::runtime_error &e) {
        break;
      }
    }
  });
}

SimpleLogger::~SimpleLogger() {
  message_channel_.close();
  thread_.join();
}

void SimpleLogger::msg(LogLevel log_level, const std::string &message) {
  if (log_level == ALL)
    throw std::runtime_error("invalid log level all to log a message");

  if (log_level < log_level_)
    return;

  std::string log_message;

  if (supports_colored_text_) {
    switch (log_level) {
      case FATAL:
        log_message = "\033[1;31m[FATAL]   \033[0m" + message + "\n";
        break;
      case ERROR:
        log_message = "\033[31m[ERROR]   \033[0m" + message + "\n";
        break;
      case WARN:
        log_message = "\033[33m[WARN]   \033[0m" + message + "\n";
        break;
      case INFO:
        log_message = "\033[32m[INFO]   \033[0m" + message + "\n";
        break;
      case DEBUG:
        log_message = "\033[1;32m[DEBUT]   \033[0m" + message + "\n";
        break;
      case ALL:break;
    }
  } else {
    switch (log_level) {
      case FATAL:
        log_message = "[FATAL] " + message + "\n";
        break;
      case ERROR:
        log_message = "[ERROR] " + message + "\n";
        break;
      case WARN:
        log_message = "[WARN] " + message + "\n";
        break;
      case INFO:
        log_message = "[INFO] " + message + "\n";
        break;
      case DEBUG:
        log_message = "[DEBUG] + " + message + "\n";
        break;
      case ALL:break;
    }
  }

  message_channel_.write(std::move(log_message));
}
}
























