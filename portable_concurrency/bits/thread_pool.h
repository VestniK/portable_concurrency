#pragma once

#include <cstdint>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <type_traits>

#include "execution.h"
#include "unique_function.hpp"
#include "closable_queue.h"

namespace portable_concurrency {
inline namespace cxx14_v1 {

namespace detail {
extern template class closable_queue<unique_function<void()>>;
} // namespace detail

/**
 * @headerfile portable_concurrency/thread_pool
 * @ingroup thread_pool
 */
class static_thread_pool {
public:
  using executor_type = detail::closable_queue<unique_function<void()>>*;

  explicit static_thread_pool(std::size_t num_threads) {
    threads_.reserve(num_threads);
    while (num_threads --> 0)
      threads_.push_back(std::thread{[this] {process_queue();}});
  }

  static_thread_pool(const static_thread_pool&) = delete;
  static_thread_pool& operator=(const static_thread_pool&) = delete;

  /// stop accepting incoming work and wait for work to drain
  ~static_thread_pool() {
    wait();
  }

  /// attach current thread to the thread pools list of worker threads
  void attach() {
    process_queue();
    wait();
  }

  /// signal all work to complete
  void stop() {
    queue_.close();
  }

  /// wait for all threads in the thread pool to complete
  void wait() {
    queue_.close();
    for (auto& thread: threads_) {
      if (thread.joinable())
        thread.join();
    }
  }

  executor_type executor() noexcept {return &queue_;}

private:
  void process_queue() {
    unique_function<void()> task;
    while (queue_.pop(task))
      task();
  }

private:
  detail::closable_queue<unique_function<void()>> queue_;
  std::vector<std::thread> threads_;
};

inline void post(static_thread_pool::executor_type exec, unique_function<void()> task) {
  exec->push(std::move(task));
}

} // inline namespace cxx14_v1

template<>
struct is_executor<cxx14_v1::static_thread_pool::executor_type>: std::true_type {};

} // namespace portable_concurrency
