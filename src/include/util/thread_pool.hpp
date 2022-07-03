#ifndef FLASHPOINT_SRC_INCLUDE_UTIL_THREAD_POOL_HPP
#define FLASHPOINT_SRC_INCLUDE_UTIL_THREAD_POOL_HPP

#include <functional>
#include <future>
#include <thread>
#include <type_traits>
#include <vector>

#include "containers/channel.hpp"

namespace flashpoint::util {

class ThreadPool {
  using Task = std::function<void()>;

public:
  explicit ThreadPool(int thread_count);
  ~ThreadPool();

  template <typename F, typename... A, typename R = std::result_of<F(A...)>>
  std::promise<R> newTask(F, A...);

private:
  void worker();

  containers::QueueChannel<Task> channel_ = {};
  std::vector<std::thread> threads_ = {};
};

} // namespace flashpoint::util

#endif // FLASHPOINT_SRC_INCLUDE_UTIL_THREAD_POOL_HPP
