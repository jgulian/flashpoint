#ifndef FLASHPOINT_SRC_INCLUDE_CONTAINERS_CHANNEL_HPP
#define FLASHPOINT_SRC_INCLUDE_CONTAINERS_CHANNEL_HPP

#include <atomic>
#include <condition_variable>
#include <deque>
#include <vector>
#include <thread>

namespace flashpoint::containers {

template <typename T> class Channel {
public:
  explicit Channel(int buffer_size = 0);
  Channel(const Channel &) = delete;

  void write(T data);
  T read();

  void close();

private:
  std::vector<T> buffer_ = {};
  std::mutex lock_ = {};
  std::atomic<bool> closed_ = false;
  std::condition_variable condition_variable_ = {};
  int read_from_ = 0, buffered_count_ = 0;
};

template <typename T> class QueueChannel {
public:
  QueueChannel() = default;
  QueueChannel(const QueueChannel &) = delete;

  void write(T data);
  T read();

  bool empty();
  void wait();

  void close();

private:
  std::deque<T> buffer_ = {};
  std::mutex lock_ = {};
  std::atomic<bool> closed_ = false;
  std::atomic<int> item_count = 0;
  std::condition_variable condition_variable_ = {};
};

}; // namespace flashpoint::containers

#endif // FLASHPOINT_SRC_INCLUDE_CONTAINERS_CHANNEL_HPP
