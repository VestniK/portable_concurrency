#pragma once

#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>

#include "future.h"
#include "shared_future.h"

namespace portable_concurrency {
inline namespace cxx14_v1 {

class timed_waiter {
private:
  struct waiter {
    std::mutex mutex;
    std::condition_variable cv;
    bool notified = false;
  };

public:
  timed_waiter() noexcept = default;

  template <typename T>
  explicit timed_waiter(future<T>& fut) : waiter_{std::make_shared<waiter>()} {
    fut.notify([waiter = waiter_] {
      std::lock_guard<std::mutex>{waiter->mutex}, waiter->notified = true;
      waiter->cv.notify_all();
    });
  }
  template <typename T>
  explicit timed_waiter(shared_future<T>& fut) : waiter_{std::make_shared<waiter>()} {
    fut.notify([waiter = waiter_] {
      std::lock_guard<std::mutex>{waiter->mutex}, waiter->notified = true;
      waiter->cv.notify_all();
    });
  }

  template <typename Rep, typename Per>
  future_status wait_for(const std::chrono::duration<Rep, Per>& dur) {
    std::unique_lock<std::mutex> lk{waiter_->mutex};
    return waiter_->cv.wait_for(lk, dur, [&] { return waiter_->notified; }) ? future_status::ready
                                                                            : future_status::timeout;
  }

  template <typename Clock, typename Dur>
  future_status wait_until(const std::chrono::time_point<Clock, Dur>& tp) {
    std::unique_lock<std::mutex> lk{waiter_->mutex};
    return waiter_->cv.wait_until(lk, tp, [&] { return waiter_->notified; }) ? future_status::ready
                                                                             : future_status::timeout;
  }

private:
  std::shared_ptr<waiter> waiter_;
};

} // namespace cxx14_v1
} // namespace portable_concurrency
