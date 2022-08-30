#include "util/env.hpp"

namespace flashpoint::util {

void LoadEnvironmentVariables() {
  auto log_level = std::getenv("LOG_LEVEL");
  Env.log_level_ = std::strlen(log_level) ? log_level : "info";
}

}