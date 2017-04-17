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

class continuation {
public:
  virtual ~continuation() = default;
  virtual void invoke() = 0;
};

template<typename Box>
class shared_state_base {
public:
  shared_state_base() = default;
  virtual ~shared_state_base() = default;

  shared_state_base(const shared_state_base&) = delete;
  shared_state_base(shared_state_base&&) = delete;

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

  void set_exception(std::exception_ptr error) {
    std::unique_lock<std::mutex> lock(mutex_);
    box_.set_exception(error);
    cv_.notify_all();
    auto continuations = std::move(continuations_);
    lock.unlock();
    for (auto& cont: continuations)
      cont->invoke();
  }

  void wait() {
    std::unique_lock<std::mutex> lock(mutex_);
    cv_.wait(lock, [this] {
      return box_.get_state() != detail::box_state::empty;
    });
  }

  template<typename Rep, typename Period>
  std::future_status wait_for(const std::chrono::duration<Rep, Period>& rel_time) {
    std::unique_lock<std::mutex> lock(mutex_);
    const bool wait_res = cv_.wait_for(lock, rel_time, [this] {
      return box_.get_state() != detail::box_state::empty;
    });
    return wait_res ? std::future_status::ready : std::future_status::timeout;
  }

  template <typename Clock, typename Duration>
  std::future_status wait_until(const std::chrono::time_point<Clock, Duration>& abs_time) {
    std::unique_lock<std::mutex> lock(mutex_);
    const bool wait_res = cv_.wait_until(lock, abs_time, [this] {
      return box_.get_state() != detail::box_state::empty;
    });
    return wait_res ? std::future_status::ready : std::future_status::timeout;
  }

  bool is_ready() {
    std::lock_guard<std::mutex> guard(mutex_);
    return box_.get_state() != detail::box_state::empty;
  }

  void add_continuation(const std::shared_ptr<continuation>& cnt) {
    {
      std::unique_lock<std::mutex> lock(mutex_);
      if (box_.get_state() == detail::box_state::empty) {
        continuations_.emplace_back(cnt);
        return;
      }
    }
    cnt->invoke();
  }

  void add_continuation(std::shared_ptr<continuation>&& cnt) {
    {
      std::unique_lock<std::mutex> lock(mutex_);
      if (box_.get_state() == detail::box_state::empty) {
        continuations_.emplace_back(std::move(cnt));
        return;
      }
    }
    cnt->invoke();
  }

protected:
  Box& box() {return box_;}

private:
  std::mutex mutex_;
  std::condition_variable cv_;
  Box box_;
  std::deque<std::shared_ptr<continuation>> continuations_;
};

template<typename T>
class shared_state: public shared_state_base<result_box<T>> {
public:
  shared_state() = default;

  T& get() {
    this->wait();
    return this->box().get();
  }
};

template<typename T>
class shared_state<T&>: public shared_state_base<result_box<std::reference_wrapper<T>>> {
public:
  shared_state() = default;

  std::reference_wrapper<T> get() {
    this->wait();
    return this->box().get();
  }
};

template<>
class shared_state<void>: public shared_state_base<result_box<void>> {
public:
  shared_state() = default;

  void get() {
    this->wait();
    this->box().get();
  }
};

} // namespace detail
} // inline namespace concurrency_v1
} // namespace experimental
