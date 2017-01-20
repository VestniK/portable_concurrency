#pragma once

#include <cassert>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <mutex>

#include "result_box.h"
#include "postponed_action.h"

namespace experimental {
inline namespace concurrency_v1 {
namespace detail {

template<typename T>
class shared_state {
public:
  shared_state() = default;
  shared_state(const shared_state&) = delete;
  shared_state(shared_state&&) = delete;

  void set_wait_action(postponed_action&& action) {
    wait_action_ = std::move(action);
  }

  template<typename... U>
  void emplace(U&&... u) {
    std::unique_lock<std::mutex> lock(mutex_);
    box_.emplace(std::forward<U>(u)...);
    cv_.notify_all();
    auto continuations = std::move(continuations_);
    lock.unlock();
    for (auto& cont: continuations)
      cont();
  }

  void set_exception(std::exception_ptr error) {
    std::unique_lock<std::mutex> lock(mutex_);
    box_.set_exception(error);
    cv_.notify_all();
    auto continuations = std::move(continuations_);
    lock.unlock();
    for (auto& cont: continuations)
      cont();
  }

  void wait() {
    std::unique_lock<std::mutex> lock(mutex_);
    wait(lock);
  }

  template<typename Rep, typename Period>
  std::future_status wait_for(const std::chrono::duration<Rep, Period>& rel_time) {
    std::unique_lock<std::mutex> lock(mutex_);
    if (wait_action_)
      return std::future_status::deferred;
    const bool wait_res = cv_.wait_for(lock, rel_time, [this] {
      return box_.get_state() != detail::box_state::empty;
    });
    return wait_res ? std::future_status::ready : std::future_status::timeout;
  }

  template <typename Clock, typename Duration>
  std::future_status wait_until(const std::chrono::time_point<Clock, Duration>& abs_time) {
    std::unique_lock<std::mutex> lock(mutex_);
    if (wait_action_)
      return std::future_status::deferred;
    const bool wait_res = cv_.wait_until(lock, abs_time, [this] {
      return box_.get_state() != detail::box_state::empty;
    });
    return wait_res ? std::future_status::ready : std::future_status::timeout;
  }

  T get() {
    std::unique_lock<std::mutex> lock(mutex_);
    wait(lock);
    return box_.get();
  }

  decltype(auto) shared_get() {
    std::unique_lock<std::mutex> lock(mutex_);
    wait(lock);
    return box_.shared_get();
  }

  bool is_ready() {
    std::lock_guard<std::mutex> guard(mutex_);
    return box_.get_state() != detail::box_state::empty;
  }

  void add_continuation(postponed_action&& cnt) {
    std::unique_lock<std::mutex> lock(mutex_);
    if (box_.get_state() != detail::box_state::empty) {
      lock.unlock();
      cnt();
    } else
      continuations_.emplace_back(std::move(cnt));
  }

private:
  void wait(std::unique_lock<std::mutex>& lock) {
    if (wait_action_)
    {
      auto action = std::move(wait_action_);
      lock.unlock();
      action();
      lock.lock();
    }
    cv_.wait(lock, [this] {
      return box_.get_state() != detail::box_state::empty;
    });
  }

private:
  std::mutex mutex_;
  std::condition_variable cv_;
  result_box<T> box_;
  postponed_action wait_action_;
  std::deque<postponed_action> continuations_;
};

} // namespace detail
} // inline namespace concurrency_v1
} // namespace experimental
