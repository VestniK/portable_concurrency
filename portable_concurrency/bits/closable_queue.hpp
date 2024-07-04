#pragma once

#include "closable_queue.h"

namespace portable_concurrency {
inline namespace cxx14_v1 {
namespace detail {

template <typename T> bool closable_queue<T>::pop(T &dest) {
  std::unique_lock<std::mutex> lock(mutex_);
  cv_.wait(lock, [this]() { return closed_ || !queue_.empty(); });
  if (closed_ && queue_.empty())
    return false;
  std::swap(dest, queue_.front());
  queue_.pop();
  return true;
}

template <typename T> void closable_queue<T>::push(T &&val) {
  std::lock_guard<std::mutex> guard(mutex_);
  if (closed_)
    return;
  queue_.emplace(std::move(val));
  cv_.notify_one();
}

template <typename T> void closable_queue<T>::close() {
  std::lock_guard<std::mutex> guard(mutex_);
  closed_ = true;
  cv_.notify_all();
}

} // namespace detail
} // namespace cxx14_v1
} // namespace portable_concurrency
