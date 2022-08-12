#ifndef FLASHPOINT_ERRORS_HPP
#define FLASHPOINT_ERRORS_HPP

enum RaftExceptionType {
  PROCESS_ALREADY_RUNNING = 0,
  PROCESS_NOT_RUNNING = 1,
  INDEX_EARLIER_THAN_SNAPSHOT = 2,
  INDEX_OUT_OF_LOG_BOUNDS = 3,
};

class RaftException : std::exception {
 private:
  RaftExceptionType type_;
  std::optional<std::string> custom_message_ = std::nullopt;

 public:
  explicit RaftException(RaftExceptionType exception_type);

  RaftException(RaftExceptionType exception_type, const std::string &custom_message);

  const char *what() const _GLIBCXX_TXN_SAFE_DYN _GLIBCXX_NOTHROW override;
};

#endif//FLASHPOINT_ERRORS_HPP
