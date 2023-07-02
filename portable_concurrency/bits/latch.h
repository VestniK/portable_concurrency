#pragma once

#include <condition_variable>
#include <cstddef>
#include <mutex>

namespace portable_concurrency {
inline namespace cxx14_v1 {

/**
 * @headerfile portable_concurrency/latch
 * @ingroup latch
 *
 * The latch class is a downward counter of type ptrdiff_t which can be used to
 * synchronize threads. The value of the counter is initialized on creation.
 * Threads may block on the latch until the counter is decremented to zero.
 * There is no possibility to increase or reset the counter, which makes the
 * latch a single-use barrier.
 */
class latch {
public:
  explicit latch(ptrdiff_t count) : counter_(count) {}

  latch(const latch &) = delete;
  latch &operator=(const latch &) = delete;

  ~latch();

  void count_down_and_wait();
  void count_down(ptrdiff_t n = 1);
  bool is_ready() const noexcept;
  void wait() const;

private:
  ptrdiff_t counter_;
  mutable unsigned waiters_ = 0;
  mutable std::mutex mutex_;
  mutable std::condition_variable cv_;
};

} // namespace cxx14_v1
} // namespace portable_concurrency
