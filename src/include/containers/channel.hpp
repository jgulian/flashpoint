#ifndef FLASHPOINT_SRC_INCLUDE_CONTAINERS_CHANNEL_HPP
#define FLASHPOINT_SRC_INCLUDE_CONTAINERS_CHANNEL_HPP

#include <atomic>
#include <condition_variable>
#include <queue>
#include <vector>
#include <thread>
#include <optional>
#include <utility>

namespace flashpoint::containers {

template<typename T>
class Channel {
 public:
  explicit Channel(int buffer_size = 0) {
    buffer_.resize(buffer_size + 1);
  }

  Channel(const Channel &) = delete;



  void write(T data) {
    if (closed_)
      throw std::runtime_error("can not write to a closed channel");

    {
      std::lock_guard<std::mutex> lock(lock_);
      int write_to = read_from_ + buffered_count_++;
      buffer_[write_to] = std::move(data);
    }

    condition_variable_.notify_one();
  }

  T read() {
    if (closed_)
      throw std::runtime_error("can not read from a closed channel");

    std::unique_lock lk(lock_);
    condition_variable_.wait(lk, [this] { return buffered_count_ != 0 || closed_; });
    if (closed_)
      throw std::runtime_error("can not read from a closed channel");

    int read_from = read_from_++;
    buffered_count_--;
    auto data = std::move(buffer_[read_from]);

    lk.unlock();
    return data;
  }



  void close() {
    if (closed_)
      throw std::runtime_error("can not close a closed channel");

    closed_ = true;
    condition_variable_.notify_all();
  }

 private:
  std::vector<T> buffer_ = {};
  std::mutex lock_ = {};
  std::atomic<bool> closed_ = false;
  std::condition_variable condition_variable_ = {};
  int read_from_ = 0, buffered_count_ = 0;
};

template<typename T>
class QueueChannel {
 public:
  QueueChannel() = default;

  QueueChannel(const QueueChannel &) = delete;



  void write(T &&data) {
    std::lock_guard<std::mutex> lock(lock_);
    accessors_++;

    if (closed_)
      throw std::runtime_error("can not write to a closed channel");

    buffer_.push(std::forward<T>(data));
    item_count_++;

    condition_variable_.notify_one();
    accessors_--;
  }

  T read() {
    std::unique_lock<std::mutex> lock(lock_);
    condition_variable_.wait(lock, [this] { return !buffer_.empty() || closed_; });
    accessors_++;

    if (closed_)
      throw std::runtime_error("can not read from a closed channel");

    auto data = std::move(buffer_.front());
    buffer_.pop();
    item_count_--;

    accessors_--;
    return std::move(data);
  }

  std::optional<T> tryRead() {
    std::lock_guard<std::mutex> lock(lock_);
    accessors_++;

    if (closed_)
      throw std::runtime_error("can not read from a closed channel");

    if (item_count_ == 0)
      return std::nullopt;

    auto data = std::move(buffer_.front());
    buffer_.pop();
    item_count_--;

    accessors_--;
    return std::move(data);
  }

  void close() {
    if (closed_)
      throw std::runtime_error("can not close a closed channel");

    closed_ = true;
    condition_variable_.notify_all();
  }

 private:
  std::atomic<int> accessors_ = 0;
  std::queue<T> buffer_ = {};
  std::mutex lock_ = {};
  std::atomic<bool> closed_ = false;
  int item_count_ = 0;
  std::condition_variable condition_variable_ = {};
};
}; // namespace flashpoint::containers

#endif // FLASHPOINT_SRC_INCLUDE_CONTAINERS_CHANNEL_HPP
