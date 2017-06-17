#pragma once

#include <chrono>
#include <condition_variable>
#include <mutex>

#include "continuation.h"

namespace portable_concurrency {
inline namespace cxx14_v1 {
namespace detail {

class wait_continuation: public detail::continuation {
public:
  wait_continuation() = default;

  void invoke() override {
    std::lock_guard<std::mutex>{mutex_}, notified_ = true;
    cv_.notify_one();
  }

  void wait() {
    std::unique_lock<std::mutex> lock{mutex_};
    cv_.wait(lock, [this]() {return notified_;});
  }

  template<typename Rep, typename Period>
  bool wait_for(std::chrono::duration<Rep, Period> rel_time) {
    std::unique_lock<std::mutex> lock{mutex_};
    return cv_.wait_for(lock, rel_time, [this]() {return notified_;});
  }

  template <typename Clock, typename Duration>
  bool wait_until(std::chrono::time_point<Clock, Duration> abs_time) {
    std::unique_lock<std::mutex> lock{mutex_};
    return cv_.wait_until(lock, abs_time, [this]() {return notified_;});
  }

private:
  std::mutex mutex_;
  std::condition_variable cv_;
  bool notified_ = false;
};

} // namespace detail
} // inline namespace cxx14_v1
} // namespace portable_concurrency
