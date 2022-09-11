#ifndef FLASHPOINT_ERRORS_HPP
#define FLASHPOINT_ERRORS_HPP

#include <optional>
#include <string>

enum RaftExceptionType {
  PROCESS_ALREADY_RUNNING = 0,
  PROCESS_NOT_RUNNING = 1,
  INDEX_EARLIER_THAN_SNAPSHOT = 2,
  INDEX_OUT_OF_LOG_BOUNDS = 3,
  ATTEMPTED_COMMIT_OF_UNKNOWN_ENTRY = 4,
  ATTEMPTED_COMMIT_OF_SNAPSHOT = 5,
  CONFIG_NOT_IN_PROPOSED_CONFIG = 6,
  CONFIG_NOT_NEXT_CONFIG = 7,
  NO_SUCH_PEER_WITH_ID = 8,
};

class RaftException : std::exception {
 private:
  RaftExceptionType exception_type_;
  std::optional<std::string> custom_message_ = std::nullopt;

 public:
  explicit RaftException(RaftExceptionType exception_type);

  RaftException(RaftExceptionType exception_type, const std::string &custom_message);

 private:
  [[nodiscard]] const char *what() const _GLIBCXX_TXN_SAFE_DYN _GLIBCXX_NOTHROW override;
};

#endif//FLASHPOINT_ERRORS_HPP
