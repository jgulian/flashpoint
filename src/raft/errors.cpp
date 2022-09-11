//
// Created by tomod on 8/12/2022.
//
#include "../include/raft/errors.hpp"

RaftException::RaftException(RaftExceptionType exception_type) : exception_type_(exception_type) {}
RaftException::RaftException(RaftExceptionType exception_type, const std::string &custom_message)
	: exception_type_(exception_type), custom_message_(custom_message) {}
const char *RaftException::what() const noexcept {
  switch (exception_type_) {
	case PROCESS_ALREADY_RUNNING: return "PROCESS_ALREADY_RUNNING";
	case PROCESS_NOT_RUNNING: return "PROCESS_NOT_RUNNING";
	case INDEX_EARLIER_THAN_SNAPSHOT: return "INDEX_EARLIER_THAN_SNAPSHOT";
	case INDEX_OUT_OF_LOG_BOUNDS: return "INDEX_OUT_OF_LOG_BOUNDS";
	case ATTEMPTED_COMMIT_OF_UNKNOWN_ENTRY:return "ATTEMPTED_COMMIT_OF_UNKNOWN_ENTRY";
	case ATTEMPTED_COMMIT_OF_SNAPSHOT:return "ATTEMPTED_COMMIT_OF_SNAPSHOT";
	case CONFIG_NOT_IN_PROPOSED_CONFIG:return "CONFIG_NOT_IN_PROPOSED_CONFIG";
	case CONFIG_NOT_NEXT_CONFIG:return "CONFIG_NOT_NEXT_CONFIG";
	case NO_SUCH_PEER_WITH_ID:return "NO_SUCH_PEER_WITH_ID";
	default: return "Unknown Raft Exception";
  }
}
