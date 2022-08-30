#ifndef FLASHPOINT_ERRORS_HPP
#define FLASHPOINT_ERRORS_HPP

#include <optional>
#include <string>

enum RaftExceptionType {
  ProcessAlreadyRunning = 0,
  ProcessNotRunning = 1,
  IndexEarlierThanSnapshot = 2,
  IndexOutOfLogBounds = 3,
  AttemptedCommitOfUnknownEntry = 4,
  AttemptedCommitOfSnapshot = 5,
  ConfigNotInProposedConfig = 6,
  ConfigNotNextConfig = 7,
  NoSuchPeerWithId = 8,
};

class RaftException : std::exception {
 private:
  RaftExceptionType exception_type_;
  std::optional<std::string> custom_message_ = std::nullopt;

 public:
  explicit RaftException(RaftExceptionType exception_type);

  RaftException(RaftExceptionType exception_type, const std::string &custom_message);

 private:
  const char *what() const _NOEXCEPT override;
};

#endif//FLASHPOINT_ERRORS_HPP
