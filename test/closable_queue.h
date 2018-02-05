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
  T pop();
  void push(T&& val);
  void close();
  void wait_empty();

private:
  std::mutex mutex_;
  std::condition_variable cv_;
  std::queue<T> queue_;
  bool closed_ = false;
};
