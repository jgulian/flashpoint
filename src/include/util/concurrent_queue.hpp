#ifndef FLASHPOINT_SRC_INCLUDE_UTIL_CONCURRENT_QUEUE_HPP_
#define FLASHPOINT_SRC_INCLUDE_UTIL_CONCURRENT_QUEUE_HPP_

#include <condition_variable>
#include <queue>

namespace flashpoint::util {

template<typename T>
class ConcurrentQueue {
 private:
  std::queue<T> base_queue_ = {};
  std::unique_ptr<std::mutex> lock_ = std::make_unique<std::mutex>();
  std::unique_ptr<std::condition_variable> cv_ = std::make_unique<std::condition_variable>();
  std::atomic<bool> running_ = true;

 public:
  ConcurrentQueue() = default;

  void Push(const T &data) {
    std::unique_lock<std::mutex> lock(*lock_);
    base_queue_.emplace(std::move(data));
    lock.unlock();
    cv_->notify_one();
  }

  void Push(T &&data) {
    std::unique_lock<std::mutex> lock(*lock_);
    base_queue_.emplace(std::move(data));
    lock.unlock();
    cv_->notify_one();
  }

  std::optional<T> TryPop() {
    if (!running_)
      return std::nullopt;

    std::unique_lock<std::mutex> lock(*lock_);
    if (base_queue_.empty())
      return std::nullopt;
    T data = std::move(base_queue_.front());
    base_queue_.pop();
    return data;
  }

  std::optional<T> Pop() {
    if (!running_)
      return std::nullopt;

    std::unique_lock<std::mutex> lock(*lock_);
    cv_->wait(lock, [this] { return !running_ || !base_queue_.empty(); });
    if (!running_)
      return std::nullopt;

    T data = std::move(base_queue_.front());
    base_queue_.pop();
    return data;
  }

  void Close() {
    running_ = false;
    cv_->notify_all();
  }
};

}

#endif //FLASHPOINT_SRC_INCLUDE_UTIL_CONCURRENT_QUEUE_HPP_
