#pragma once

#include "closable_queue.h"

template<typename T>
T closable_queue<T>::pop() {
  std::unique_lock<std::mutex> lock(mutex_);
  cv_.wait(lock, [this]() {return closed_ || !queue_.empty();});
  if (closed_)
    throw queue_closed();
  T res = std::move(queue_.front());
  queue_.pop();
  bool empty = queue_.empty();
  lock.unlock();
  if (empty)
    cv_.notify_all();
  return res;
}

template<typename T>
void closable_queue<T>::push(T&& val) {
  std::lock_guard<std::mutex> guard(mutex_);
  if (closed_)
    throw queue_closed();
  queue_.emplace(std::move(val));
  cv_.notify_one();
}

template<typename T>
void closable_queue<T>::close() {
  std::lock_guard<std::mutex> guard(mutex_);
  closed_ = true;
  cv_.notify_all();
}

template<typename T>
void closable_queue<T>::wait_empty() {
  std::unique_lock<std::mutex> lock(mutex_);
  cv_.wait(lock, [this] {return queue_.empty();});
}
