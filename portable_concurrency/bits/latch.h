#pragma once

#include <cassert>
#include <cstddef>
#include <condition_variable>
#include <mutex>

namespace portable_concurrency {
inline namespace cxx14_v1 {

class latch {
public:
  explicit latch(ptrdiff_t count): counter_(count) {}

  latch(const latch&) = delete;
  latch& operator=(const latch&) = delete;

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

} // inline namespace cxx14_v1
} // namespace portable_concurrency
