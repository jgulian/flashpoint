//
// Created by tomod on 8/12/2022.
//
#include "../include/raft/errors.hpp"

RaftException::RaftException(RaftExceptionType exception_type) : exception_type_(exception_type) {}
RaftException::RaftException(RaftExceptionType exception_type, const std::string &custom_message)
    : exception_type_(exception_type), custom_message_(custom_message) {}
const char *RaftException::what() const _GLIBCXX_TXN_SAFE_DYN _GLIBCXX_NOTHROW {
  if (custom_message_.has_value()) return custom_message_->c_str();

  switch (exception_type_) {
    case ProcessAlreadyRunning: return "";
    case ProcessNotRunning: return "";
    case IndexEarlierThanSnapshot: return "";
    case IndexOutOfLogBounds: return "";
    default: return "";
  }
}
