#ifndef FLASHPOINT_RAFT_RANDOM_H_
#define FLASHPOINT_RAFT_RANDOM_H_

#include <chrono>
#include <random>

namespace flashpoint::raft {

using Time = std::chrono::time_point<std::chrono::system_clock>;

}

#endif // FLASHPOINT_RAFT_RANDOM_H_
