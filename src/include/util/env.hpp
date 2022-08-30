#ifndef FLASHPOINT_SRC_INCLUDE_UTIL_ENV_HPP_
#define FLASHPOINT_SRC_INCLUDE_UTIL_ENV_HPP_

#include <string>

namespace flashpoint::util {

struct {
  std::string log_level_;
} Env;

void LoadEnvironmentVariables();

}// namespace flashpoint::util

#endif//FLASHPOINT_SRC_INCLUDE_UTIL_ENV_HPP_
