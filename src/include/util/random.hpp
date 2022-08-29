#ifndef FLASHPOINT_SRC_INCLUDE_UTIL_RANDOM_H_
#define FLASHPOINT_SRC_INCLUDE_UTIL_RANDOM_H_

#include <chrono>
#include <memory>
#include <random>

namespace flashpoint::util {

class Random {
 public:
  virtual float random() = 0;
  virtual int randomInt(int a, int b) = 0;
  virtual std::shared_ptr<Random> seedRandom() = 0;

  template<class Duration>
  Duration randomDurationBetween(Duration min, Duration max) {
    return std::chrono::duration_cast<Duration>(min + (max - min) * random());
  }
};

class MTRandom : public Random {
 private:
  std::mt19937 generator_;

 public:
  MTRandom() : generator_(std::random_device()()) {}

  explicit MTRandom(std::seed_seq &seed) : generator_(seed) {}

  float random() override { return std::uniform_real_distribution<float>(0, 1)(generator_); }

  int randomInt(int a, int b) override { return std::uniform_int_distribution(a, b)(generator_); }

  std::seed_seq generateSeed() {
    std::vector<unsigned long> seed = {};
    for (int i = 0; i < 8; i++) seed.push_back(randomInt(0, 1000));
    return std::seed_seq(seed.begin(), seed.end());
  }

  std::shared_ptr<Random> seedRandom() override {
    auto seed = generateSeed();
    return std::make_shared<MTRandom>(seed);
  }
};

}// namespace flashpoint::util
#endif//FLASHPOINT_SRC_INCLUDE_UTIL_RANDOM_H_
