#include "containers/channel.hpp"

namespace flashpoint::containers {

template <typename T> Channel<T>::Channel(int buffer_size) {
  buffer_.resize(buffer_size + 1);
}

template <typename T> void Channel<T>::write(T data) {
  if (closed_)
    throw std::runtime_error("can not write to a closed channel");

  {
    std::lock_guard<std::mutex> lock(lock_);
    int write_to = read_from_ + buffered_count_++;
    buffer_[write_to] = std::move(data);
  }

  condition_variable_.notify_one();
}
template <typename T> T Channel<T>::read() {
  if (closed_)
    throw std::runtime_error("can not read from a closed channel");

  std::unique_lock lk(lock_);
  condition_variable_.wait(lk, [this] { return buffered_count_ != 0; });

  int read_from = read_from_++;
  buffered_count_--;
  auto data = std::move(buffer_[read_from]);

  lk.unlock();
  return data;
}

template <typename T> void Channel<T>::close() { closed_ = true; }

template <typename T> void QueueChannel<T>::write(T data) {
  if (closed_)
    throw std::runtime_error("can not write to a closed channel");

  {
    std::lock_guard<std::mutex> lock(lock_);
    buffer_.push_back(std::move(data));
    item_count++;
  }

  condition_variable_.notify_one();
}
template <typename T> T QueueChannel<T>::read() {
  if (closed_)
    throw std::runtime_error("can not read from a closed channel");

  std::unique_lock lk(lock_);
  condition_variable_.wait(lk, [this] { return !buffer_.empty(); });

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

template <typename T> bool QueueChannel<T>::empty() {
  return item_count == 0;
}
template <typename T> void QueueChannel<T>::wait() {
  while(!empty())
    std::this_thread::yield();
}

template <typename T> void QueueChannel<T>::close() {
  if (closed_)
    throw std::runtime_error("can not close a closed channel");

  closed_ = true;
}

} // namespace flashpoint::containers