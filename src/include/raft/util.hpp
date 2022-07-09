#ifndef FLASHPOINT_RAFT_RANDOM_H_
#define FLASHPOINT_RAFT_RANDOM_H_

#include <chrono>
#include <random>

namespace flashpoint::raft {

using Time = std::chrono::time_point<std::chrono::system_clock>;

class Random {
 public:
  Random();



  float randomUnitUniform();

  template<class Duration>
  Duration randomDurationBetween(Duration min, Duration max) {
    return std::chrono::duration_cast<Duration>(min + (max - min) * randomUnitUniform());
  }


 private:
  std::mt19937 random_number_generator_;
  std::uniform_real_distribution<float> unit_uniform_distribution_;
};
}

#endif // FLASHPOINT_RAFT_RANDOM_H_
