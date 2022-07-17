#ifndef FLASHPOINT_SRC_INCLUDE_UTIL_CONTAINERS_HPP_
#define FLASHPOINT_SRC_INCLUDE_UTIL_CONTAINERS_HPP_

#include <utility>

template <typename K, typename V>
struct SetMapKeyComparator {
  bool operator()(K &set, const std::pair<K, V>& map) {
    return set < map.first;
  }

  bool operator()(const std::pair<K, V>& map, K &set) {
    return map.first < set;
  }
};

#endif //FLASHPOINT_SRC_INCLUDE_UTIL_CONTAINERS_HPP_
