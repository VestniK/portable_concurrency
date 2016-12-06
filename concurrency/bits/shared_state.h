#pragma once

#include <cassert>
#include <chrono>
#include <condition_variable>
#include <mutex>

#include "result_box.h"

namespace concurrency {

namespace detail {

template<typename T>
class shared_state {
public:
  shared_state() = default;
  shared_state(const shared_state&) = delete;
  shared_state(shared_state&&) = delete;

  template<typename U>
  void set_value(U&& u) {
    std::lock_guard<std::mutex> guard(mutex);
    if (retrieved)
      throw future_error(future_errc::future_already_retrieved);
    box.set_value(std::forward<U>(u));
    cv.notify_all();
  }

  void set_exception(std::exception_ptr error) {
    std::lock_guard<std::mutex> guard(mutex);
    if (retrieved)
      throw future_error(future_errc::future_already_retrieved);
    box.set_exception();
    cv.notify_all();
  }

  void wait() {
    std::unique_lock<std::mutex> lock(mutex);
    cv.wait(lock, [this] {
      return box.get_state() != detail::box_state::empty;
    });
  }

  template<typename Rep, typename Period>
  future_status wait_for(const std::chrono::duration<Rep, Period>& rel_time) {
    std::unique_lock<std::mutex> lock(mutex);
    const bool wait_res = cv.wait_for(lock, rel_time, [this] {
      return box.get_state() != detail::box_state::empty;
    });
    return wait_res ? future_status::ready : future_status::timeout;
  }

  template <typename Clock, typename Duration>
  future_status wait_until(const std::chrono::time_point<Clock, Duration>& abs_time) const {
    std::unique_lock<std::mutex> lock(mutex);
    const bool wait_res = cv.wait_until(lock, abs_time, [this] {
      return box.get_state() != detail::box_state::empty;
    });
    return wait_res ? future_status::ready : future_status::timeout;
  }

  T get() {
    std::unique_lock<std::mutex> lock(mutex);
    cv.wait(lock, [this] {return box.get_state() != detail::box_state::empty;});
    if (retrieved)
      throw future_error(future_errc::future_already_retrieved);
    retrieved = true;
    return box.get();
  }

private:
  std::mutex mutex;
  std::condition_variable cv;
  result_box<T> box;
  bool retrieved = false;
};

} // namespace detail

} // namespace concurrency
