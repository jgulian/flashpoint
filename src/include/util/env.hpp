#ifndef FLASHPOINT_SRC_INCLUDE_UTIL_ENV_HPP_
#define FLASHPOINT_SRC_INCLUDE_UTIL_ENV_HPP_

#include <cstring>
#include <string>

namespace flashpoint::util {

struct EnvironmentVariables {
  std::string log_level_;
};


void LoadEnvironmentVariables();

}// namespace flashpoint::util

#endif//FLASHPOINT_SRC_INCLUDE_UTIL_ENV_HPP_
