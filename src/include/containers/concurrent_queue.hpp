#ifndef FLASHPOINT_CONCURRENT_LINKED_LIST_HPP
#define FLASHPOINT_CONCURRENT_LINKED_LIST_HPP

#include <atomic>
#include <memory>

namespace flashpoint::containers {

template<class T>
class ConcurrentQueue {
  struct ConcurrentQueueNode {
    std::shared_ptr<T> data;
    std::atomic<std::shared_ptr<ConcurrentQueueNode>> next;
  };

  std::atomic<std::shared_ptr<ConcurrentQueueNode>> head_;

 public:
  std::shared_ptr<T> pop() {
    auto back = head_;
    if (back == nullptr) return nullptr;
  }

  void push(std::shared_ptr<T> data) {
    while (true) {
      ConcurrentQueueNode node = {data, head_, true};
      if (head_.compare_exchange_strong(head_, node)) break;
    }
  }
};

}// namespace flashpoint::containers

#endif//FLASHPOINT_CONCURRENT_LINKED_LIST_HPP
