#pragma once

#include <cassert>
#include <condition_variable>
#include <mutex>

namespace experimental {
inline namespace concurrency_v1 {

class latch {
public:
  explicit latch(ptrdiff_t count): counter_(count) {}

  latch(const latch&) = delete;
  latch& operator=(const latch&) = delete;

  // TODO: implement requirement to wait for all count_down_and_wait
  // and wait functions returned before leaving the destructor.
  ~latch() = default;

  void count_down_and_wait() {
    std::unique_lock<std::mutex> lock{mutex_};
    assert(counter_ > 0);
    if (--counter_ == 0) {
      lock.unlock();
      cv_.notify_all();
      return;
    }
    cv_.wait(lock, [this]() {return counter_ == 0;});
  }

  void count_down(ptrdiff_t n = 1) {
    {
      std::lock_guard<std::mutex> lock{mutex_};
      assert(counter_ >= n);
      assert(n >= 0);
      counter_ -= n;
      if (counter_ > 0)
        return;
    }
    cv_.notify_all();
  }

  bool is_ready() const noexcept {
    std::lock_guard<std::mutex> lock{mutex_};
    return counter_ == 0;
  }

  void wait() const {
    std::unique_lock<std::mutex> lock{mutex_};
    cv_.wait(lock, [this]() {return counter_ == 0;});
  }

private:
  ptrdiff_t counter_;
  mutable std::mutex mutex_;
  mutable std::condition_variable cv_;
};

} // inline namespace concurrency_v1
} // namespace experimental
