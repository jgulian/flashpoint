#ifndef FLASHPOINT_SRC_INCLUDE_CONTAINERS_CHANNEL_HPP
#define FLASHPOINT_SRC_INCLUDE_CONTAINERS_CHANNEL_HPP

#include <atomic>
#include <condition_variable>
#include <deque>
#include <vector>
#include <thread>

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



  void write(T data) {
    if (closed_)
      throw std::runtime_error("can not write to a closed channel");

    {
      std::lock_guard<std::mutex> lock(lock_);
      buffer_.push_back(std::move(data));
      item_count++;
    }

    condition_variable_.notify_one();
  }

  T read() {
    if (closed_)
      throw std::runtime_error("can not read from a closed channel");

    std::unique_lock lk(lock_);
    condition_variable_.wait(lk, [this] { return !buffer_.empty() || closed_; });
    if (closed_)
      throw std::runtime_error("can not read from a closed channel");

    T data;
    {
      std::lock_guard<std::mutex> lock(lock_);
      data = std::move(buffer_.front());
      buffer_.pop_front();
      item_count--;
    }

    lk.unlock();
    return std::move(data);
  }



  bool empty() {
    return item_count == 0;
  }

  void wait() {
    while (!empty())
      std::this_thread::yield();
  }



  void close() {
    if (closed_)
      throw std::runtime_error("can not close a closed channel");

    closed_ = true;
    condition_variable_.notify_all();
  }

 private:
  std::deque<T> buffer_ = {};
  std::mutex lock_ = {};
  std::atomic<bool> closed_ = false;
  std::atomic<int> item_count = 0;
  std::condition_variable condition_variable_ = {};
};
}; // namespace flashpoint::containers

#endif // FLASHPOINT_SRC_INCLUDE_CONTAINERS_CHANNEL_HPP
