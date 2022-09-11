#include "util/env.hpp"

namespace flashpoint::util {

void LoadEnvironmentVariables() {
  auto log_level = std::getenv("LOG_LEVEL");
  //env.log_level = std::strlen(log_level) ? log_level : "info";
}

}