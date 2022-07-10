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

  template <typename F, typename... A, typename R = std::invoke_result_t<std::decay_t<F>, std::decay_t<A>...>>
  std::promise<R> newTask(const F& f, const A& ...a) {
    std::promise<R> promise;
    channel_.write([&promise, f, a...](){
      if constexpr (std::is_void_v<R>) {
        f(a...);
        promise.set_value();
      } else {
        promise.set_value(f(a...));
      }
    });
    return promise;
  }

private:
  void worker();

  std::atomic<bool> running_ = true;
  containers::QueueChannel<Task> channel_ = {};
  std::vector<std::thread> threads_ = {};
};

} // namespace flashpoint::util

#endif // FLASHPOINT_SRC_INCLUDE_UTIL_THREAD_POOL_HPP
