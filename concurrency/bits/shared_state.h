#pragma once

#include <cassert>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <mutex>

#include "result_box.h"

namespace experimental {
inline namespace concurrency_v1 {
namespace detail {

class action {
public:
  virtual ~action() = default;
  virtual bool is_executed() const noexcept = 0;
  virtual void invoke() = 0;
};

class continuation {
public:
  virtual ~continuation() = default;
  virtual void invoke() = 0;
};

template<typename T>
class shared_state {
public:
  shared_state() = default;
  virtual ~shared_state() = default;

  shared_state(const shared_state&) = delete;
  shared_state(shared_state&&) = delete;

  // threadsafe
  template<typename... U>
  void emplace(U&&... u) {
    std::unique_lock<std::mutex> lock(mutex_);
    box_.emplace(std::forward<U>(u)...);
    cv_.notify_all();
    auto continuations = std::move(continuations_);
    lock.unlock();
    for (auto& cont: continuations)
      cont->invoke();
  }

  // threadsafe
  void set_exception(std::exception_ptr error) {
    std::unique_lock<std::mutex> lock(mutex_);
    box_.set_exception(error);
    cv_.notify_all();
    auto continuations = std::move(continuations_);
    lock.unlock();
    for (auto& cont: continuations)
      cont->invoke();
  }

  // threadsafe
  void wait() {
    if (deferred_action_)
      deferred_action_->invoke();
    std::unique_lock<std::mutex> lock(mutex_);
    cv_.wait(lock, [this] {
      return box_.get_state() != detail::box_state::empty;
    });
  }

  // threadsafe
  template<typename Rep, typename Period>
  std::future_status wait_for(const std::chrono::duration<Rep, Period>& rel_time) {
    if (deferred_action_ && !deferred_action_->is_executed())
      return std::future_status::deferred;
    std::unique_lock<std::mutex> lock(mutex_);
    const bool wait_res = cv_.wait_for(lock, rel_time, [this] {
      return box_.get_state() != detail::box_state::empty;
    });
    return wait_res ? std::future_status::ready : std::future_status::timeout;
  }

  // threadsafe
  template <typename Clock, typename Duration>
  std::future_status wait_until(const std::chrono::time_point<Clock, Duration>& abs_time) {
    if (deferred_action_ && !deferred_action_->is_executed())
      return std::future_status::deferred;
    std::unique_lock<std::mutex> lock(mutex_);
    const bool wait_res = cv_.wait_until(lock, abs_time, [this] {
      return box_.get_state() != detail::box_state::empty;
    });
    return wait_res ? std::future_status::ready : std::future_status::timeout;
  }

  // threadsafe
  T get() {
    if (deferred_action_)
      deferred_action_->invoke();
    std::unique_lock<std::mutex> lock(mutex_);
    cv_.wait(lock, [this] {
      return box_.get_state() != detail::box_state::empty;
    });
    return box_.get();
  }

  // threadsafe
  decltype(auto) shared_get() {
    if (deferred_action_)
      deferred_action_->invoke();
    std::unique_lock<std::mutex> lock(mutex_);
    cv_.wait(lock, [this] {
      return box_.get_state() != detail::box_state::empty;
    });
    return box_.shared_get();
  }

  // threadsafe
  bool is_ready() {
    std::lock_guard<std::mutex> guard(mutex_);
    return box_.get_state() != detail::box_state::empty;
  }

  // threadsafe
  void add_continuation(const std::shared_ptr<continuation>& cnt) {
    std::unique_lock<std::mutex> lock(mutex_);
    if (box_.get_state() != detail::box_state::empty) {
      lock.unlock();
      cnt->invoke();
    } else
      continuations_.emplace_back(std::move(cnt));
  }

  // non-threadsafe must be called once as close as possible to constructor before
  // sharing references to this object across different threads
  void set_deferred_action(std::shared_ptr<action>&& action) {
    deferred_action_ = action;
  }

  // threadsafe
  std::shared_ptr<action> get_deferred_action() const {
    return deferred_action_;
  }

private:
  std::mutex mutex_;
  std::condition_variable cv_;
  result_box<T> box_;
  std::shared_ptr<action> deferred_action_;
  std::deque<std::shared_ptr<continuation>> continuations_;
};

} // namespace detail
} // inline namespace concurrency_v1
} // namespace experimental
