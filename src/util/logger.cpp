#include "util/logger.hpp"

namespace flashpoint::util {

void SimpleLogger::msg(LogLevel log_level, const std::string &message) {
  if (log_level == LogLevel::ALL) throw std::runtime_error("invalid log level all to log a message");

  if (log_level < log_level_) return;

  std::string log_message;

  if (supports_colored_text_) {
	switch (log_level) {
	  case LogLevel::FATAL: log_message = "\033[1;31m[FATAL]   \033[0m" + message + "\n";
		break;
	  case LogLevel::ERROR2: log_message = "\033[31m[ERROR]   \033[0m" + message + "\n";
		break;
	  case LogLevel::WARN: log_message = "\033[33m[WARN]   \033[0m" + message + "\n";
		break;
	  case LogLevel::INFO: log_message = "\033[32m[INFO]   \033[0m" + message + "\n";
		break;
	  case LogLevel::DEBUG: log_message = "\033[1;32m[DEBUT]   \033[0m" + message + "\n";
		break;
	  case LogLevel::ALL: break;
	}
  } else {
	switch (log_level) {
	  case LogLevel::FATAL: log_message = "[FATAL] " + message + "\n";
		break;
	  case LogLevel::ERROR2: log_message = "[ERROR] " + message + "\n";
		break;
	  case LogLevel::WARN: log_message = "[WARN] " + message + "\n";
		break;
	  case LogLevel::INFO: log_message = "[INFO] " + message + "\n";
		break;
	  case LogLevel::DEBUG: log_message = "[DEBUG] + " + message + "\n";
		break;
	  case LogLevel::ALL: break;
	}
  }

  queue_.Push(log_message);
}

bool SimpleLogger::worker() {
  if (!running_->load()) {
	running_->store(true);
	thread_ = std::make_unique<std::thread>([this]() {
	  while (running_->load()) {
		auto message = queue_.Pop();
		while (message.has_value()) {
		  std::cout << message.value();
		  message = queue_.Pop();
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	  }
	});
	return true;
  } else {
	return false;
  }
}
bool SimpleLogger::Kill() {
  if (running_->load()) {
	running_->store(false);
	thread_->join();
	return true;
  }
  return false;
}

static std::shared_ptr<Logger> active_logger;

void SetLogger(std::shared_ptr<Logger> logger) {
  active_logger = std::move(logger);
}
std::shared_ptr<Logger> GetLogger() {
  return active_logger;
}
}// namespace flashpoint::util
