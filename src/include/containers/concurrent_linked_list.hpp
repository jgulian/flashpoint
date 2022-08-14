#ifndef FLASHPOINT_CONCURRENT_LINKED_LIST_HPP
#define FLASHPOINT_CONCURRENT_LINKED_LIST_HPP

#include <memory>

template<class T>
class ConcurrentSinglyLinkedList {
  struct ConcurrentSinglyLinkedListNode {
    std::shared_ptr<T> data;
    std::atomic<std::shared_ptr<ConcurrentSinglyLinkedListNode>> next;
  };

  std::atomic<std::shared_ptr<ConcurrentSinglyLinkedListNode>> head_, tail_;

 public:
  void addToBack(std::shared_ptr<T> data) {}
};

#endif//FLASHPOINT_CONCURRENT_LINKED_LIST_HPP
