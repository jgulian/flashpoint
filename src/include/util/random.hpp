#ifndef FLASHPOINT_SRC_INCLUDE_UTIL_RANDOM_H_
#define FLASHPOINT_SRC_INCLUDE_UTIL_RANDOM_H_

#include <chrono>
#include <random>

namespace flashpoint::util {

template<class Generator>
class Random {
 public:
  Random() : generator_(std::random_device()()) {}

  explicit Random(std::seed_seq seed) : generator_(seed) {}


  std::seed_seq generateSeed() {
    std::vector<unsigned long> seed = {};
    for (int i = 0; i < 8; i++)
      seed.push_back(unsigned_random_(generator_));
    return std::seed_seq(seed.begin(), seed.end());
  }

  Random generateRandom() {
    return Random(generateSeed());
  }

  float generateUnitUniform() {
    return unit_uniform_(generator_);
  }

  template<class Duration>
  Duration generateDurationBetween(Duration min, Duration max) {
    return std::chrono::duration_cast<Duration>(min + (max - min) * generateUnitUniform());
  }


 private:
  Generator generator_;

  std::uniform_int_distribution<unsigned int>
      unsigned_random_ = std::uniform_int_distribution<unsigned int>(0, std::numeric_limits<unsigned int>::max());
  std::uniform_real_distribution<float> unit_uniform_ = std::uniform_real_distribution<float>(0, 1);
};

using DefaultRandom = Random<std::mt19937>;

}

#endif //FLASHPOINT_SRC_INCLUDE_UTIL_RANDOM_H_
