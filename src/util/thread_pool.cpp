#include "util/thread_pool.hpp"

#include <iostream>

namespace flashpoint::util {

ThreadPool::ThreadPool(int thread_count) {
  for (auto i = 0; i < thread_count; i++)
    threads_.emplace_back(std::thread(&ThreadPool::worker, this));
}
ThreadPool::~ThreadPool() {
  running_ = false;
  channel_.close();
  for (auto &thread : threads_)
    thread.join();
}

void ThreadPool::worker() {
  while (running_) {
    try {
      auto task = channel_.read();
      task();
    } catch (const std::runtime_error& e) {}
  }
}

}