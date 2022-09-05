#ifndef FLASHPOINT_SRC_INCLUDE_UTIL_CONCURRENT_QUEUE_HPP_
#define FLASHPOINT_SRC_INCLUDE_UTIL_CONCURRENT_QUEUE_HPP_

#include <queue>

namespace flashpoint::util {

template<typename T>
class ConcurrentQueue {
 private:
  std::queue<T> base_queue_ = {};
  std::unique_ptr<std::mutex> lock_;

 public:
  ConcurrentQueue() = default;
  ConcurrentQueue(ConcurrentQueue &&other) noexcept = default;

  void Push(const T &data) {
	std::lock_guard<std::mutex> lock_guard(*lock_);
	base_queue_.push(data);
  }

  void Push(T &&data) {
	std::lock_guard<std::mutex> lock_guard(*lock_);
	base_queue_.push(data);
  }

  std::optional<T> Pop() {
	std::lock_guard<std::mutex> lock_guard(*lock_);
	if (base_queue_.empty())
	  return std::nullopt;
	T data = base_queue_.front();
	base_queue_.pop();
	return data;
  }
};

}

#endif //FLASHPOINT_SRC_INCLUDE_UTIL_CONCURRENT_QUEUE_HPP_