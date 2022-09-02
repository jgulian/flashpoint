#ifndef FLASHPOINT_CONCURRENT_LINKED_LIST_HPP
#define FLASHPOINT_CONCURRENT_LINKED_LIST_HPP

#include <atomic>
#include <memory>

namespace flashpoint::containers {

template<class T>
class ConcurrentLinkedList {
  struct ConcurrentLinkedListNode {
    std::shared_ptr<T> data;
    std::atomic<std::shared_ptr<ConcurrentLinkedListNode>> next;
  };

  std::atomic<std::shared_ptr<ConcurrentLinkedListNode>> head_;
  std::atomic<std::shared_ptr<ConcurrentLinkedListNode>> tail_;

 public:
  bool insert(T &&data) {
    auto node = std::make_shared<ConcurrentLinkedListNode>(std::make_shared<T>(data), nullptr);
    std::shared_ptr<ConcurrentLinkedListNode> right_node, left_node;

    do {
      right_node = search()
    } while (true); }

  bool insert(T data) { insert(std::move(data)); }

  std::shared_ptr<T> pop() {
    auto back = head_;
    if (back == nullptr) return nullptr;
  }

  void push(std::shared_ptr<T> data) {
    while (true) {
      ConcurrentLinkedListNode node = {data, head_, true};
      if (head_.compare_exchange_strong(head_, node)) break;
    }
  }

 private:
  std::pair<std::shared_ptr<T>, std::shared_ptr<T>> search(std::shared_ptr<T> search_key) {

  }
};

}// namespace flashpoint::containers

#endif//FLASHPOINT_CONCURRENT_LINKED_LIST_HPP
