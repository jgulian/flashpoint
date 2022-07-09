#include "util/thread_pool.hpp"

namespace flashpoint::util {

ThreadPool::ThreadPool(int thread_count) {
  for (auto i = 0; i < thread_count; i++)
    threads_.emplace_back(std::thread(&ThreadPool::worker, this));
}
ThreadPool::~ThreadPool() {
  for (auto &thread : threads_)
    thread.join();
}

void ThreadPool::worker() {
  auto task = channel_.read();
  task();
}

}