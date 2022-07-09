#include "raft/util.hpp"

namespace flashpoint::raft {

Random::Random() {
  std::random_device rd;
  random_number_generator_ = std::mt19937(rd());
  unit_uniform_distribution_ = std::uniform_real_distribution<float>(0, 1);
}
float Random::randomUnitUniform() {
  return unit_uniform_distribution_(random_number_generator_);
}

}
