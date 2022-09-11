#ifndef FLASHPOINT_SRC_INCLUDE_UTIL_RANDOM_HPP_
#define FLASHPOINT_SRC_INCLUDE_UTIL_RANDOM_HPP_

#include <chrono>
#include <memory>
#include <random>

namespace flashpoint::util {

class Random {
 public:
  virtual float RandomFloat() = 0;
  virtual int RandomInt(int a, int b) = 0;
  virtual std::shared_ptr<Random> SeedRandom() = 0;

  template<class Duration>
  Duration RandomDurationBetween(Duration min, Duration max) {
	return std::chrono::duration_cast<Duration>(min + (max - min) * RandomFloat());
  }
};

class MtRandom : public Random {
 private:
  std::mt19937 generator_;

 public:
  MtRandom() : generator_(std::random_device()()) {}

  explicit MtRandom(std::seed_seq &seed) : generator_(seed) {}

  float RandomFloat() override { return std::uniform_real_distribution<float>(0, 1)(generator_); }

  int RandomInt(int a, int b) override { return std::uniform_int_distribution(a, b)(generator_); }

  std::seed_seq GenerateSeed() {
	std::vector<unsigned long> seed = {};
	for (int i = 0; i < 8; i++) seed.push_back(RandomInt(0, 1000));
	return std::seed_seq(seed.begin(), seed.end());
  }

  std::shared_ptr<Random> SeedRandom() override {
	auto seed = GenerateSeed();
	return std::make_shared<MtRandom>(seed);
  }
};

}// namespace flashpoint::util
#endif//FLASHPOINT_SRC_INCLUDE_UTIL_RANDOM_HPP_
