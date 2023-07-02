#pragma once

#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>

#include "future.h"
#include "shared_future.h"

namespace portable_concurrency {
inline namespace cxx14_v1 {

/**
 * @ingroup timed_waiter_hdr
 * @brief Class to wait on @ref future or `shared_future` with timeout
 *
 * This class is intended to become complete replacement for `wait_for` and
 * `wait_until` member functions of @ref future and `shared_future` classes.
 * Both of those functions are rarely used but prevent internal implementation
 * optimizations.
 */
class timed_waiter {
private:
  struct waiter {
    std::mutex mutex;
    std::condition_variable cv;
    bool notified = false;
  };

public:
  /**
   * @brief Constructs `timed_waiter` in some unspecified state.
   *
   * No operations are allowed on the default constructed object except
   * copy/move assignment and destruction.
   */
  timed_waiter() noexcept = default;

  /**
   * @brief Constructs `timed_waiter` associated with @a fut object.
   */
  template <typename T>
  explicit timed_waiter(future<T> &fut) : waiter_{std::make_shared<waiter>()} {
    fut.notify([waiter = waiter_] {
      {
        std::lock_guard<std::mutex> lock{waiter->mutex};
        waiter->notified = true;
      }
      waiter->cv.notify_all();
    });
  }
  /**
   * @brief Constructs `timed_waiter` associated with @a fut object.
   */
  template <typename T>
  explicit timed_waiter(shared_future<T> &fut)
      : waiter_{std::make_shared<waiter>()} {
    fut.notify([waiter = waiter_] {
      {
        std::lock_guard<std::mutex> lock{waiter->mutex};
        waiter->notified = true;
      }
      waiter->cv.notify_all();
    });
  }

  /**
   * @brief Waits for the result, returns if it is not available for the
   * specified timeout duration.
   */
  template <typename Rep, typename Per>
  future_status wait_for(const std::chrono::duration<Rep, Per> &dur) {
    std::unique_lock<std::mutex> lk{waiter_->mutex};
    return waiter_->cv.wait_for(lk, dur, [&] { return waiter_->notified; })
               ? future_status::ready
               : future_status::timeout;
  }

  /**
   * @brief Waits for the result, returns if it is not available until specified
   * time point has been reached.
   */
  template <typename Clock, typename Dur>
  future_status wait_until(const std::chrono::time_point<Clock, Dur> &tp) {
    std::unique_lock<std::mutex> lk{waiter_->mutex};
    return waiter_->cv.wait_until(lk, tp, [&] { return waiter_->notified; })
               ? future_status::ready
               : future_status::timeout;
  }

private:
  std::shared_ptr<waiter> waiter_;
};

} // namespace cxx14_v1
} // namespace portable_concurrency
