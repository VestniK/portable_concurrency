#pragma once

#include <cassert>
#include <condition_variable>
#include <mutex>

#include "result_box.h"

namespace concurrency {

namespace detail {

template<typename T>
struct shared_state {
  std::mutex mutex;
  std::condition_variable cv;
  result_box<T> box;
};

} // namespace detail

} // namespace concurrency
