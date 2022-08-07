#ifndef FLASHPOINT_SRC_INCLUDE_CONTAINERS_SUBJECT_HPP_
#define FLASHPOINT_SRC_INCLUDE_CONTAINERS_SUBJECT_HPP_

#include <functional>
#include <map>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <utility>

namespace flashpoint::containers {

using SubId = unsigned long;

template<typename T>
class Subject {

  Subject() {
    current_data_ = std::nullopt;
  }

  Subject(T data) {
    current_data_ = data;
  }

  void next(T data) {
    std::shared_lock<std::shared_mutex> lk(lock_);
  }

  SubId subscribe(std::function<void(const T&)> sub) {
    std::unique_lock<std::shared_mutex> lk(lock_);
    auto sub_id = id_counter_++;
    subscriptions_.insert(sub_id, sub);
    return sub_id;
  }

  void unsubscribe(SubId id) {
    std::unique_lock<std::shared_mutex> lk(lock_);
    if (!subscriptions_.contains(id))
      throw std::runtime_error("can not unsubscribe from unknown id");
    subscriptions_.erase(id);
  }

  std::optional<T> current_data_;
  SubId id_counter_ = 0;
  std::map<SubId, std::function<void(const T&)>> subscriptions_ = {};
  std::shared_mutex lock_ = {};
};

}

#endif //FLASHPOINT_SRC_INCLUDE_CONTAINERS_SUBJECT_HPP_
