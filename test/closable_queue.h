#pragma once

#include <condition_variable>
#include <exception>
#include <mutex>
#include <queue>

class queue_closed: public std::exception {
public:
  const char* what() const noexcept override {return "queue is closed";}
};

template<typename T>
class closable_queue {
public:
  T pop() {
    std::unique_lock<std::mutex> lock(mutex_);
    cv_.wait(lock, [this]() {return closed_ || !queue_.empty();});
    if (closed_)
      throw queue_closed();
    T res = std::move(queue_.front());
    queue_.pop();
    return res;
  }

  template<typename... A>
  void emplace(A&&... a) {
    std::lock_guard<std::mutex> guard(mutex_);
    if (closed_)
      throw queue_closed();
    queue_.emplace(std::forward<A>(a)...);
    cv_.notify_one();
  }
  
  void close() {
    std::lock_guard<std::mutex> guard(mutex_);
    closed_ = true;
    cv_.notify_all();
  }

private:
  std::mutex mutex_;
  std::condition_variable cv_;
  std::queue<T> queue_;
  bool closed_ = false;
};
