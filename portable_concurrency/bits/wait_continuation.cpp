#include "wait_continuation.h"

namespace portable_concurrency {
inline namespace cxx14_v1 {
namespace detail {

void wait_continuation::operator() () {
  std::lock_guard<std::mutex>{mutex_}, notified_ = true;
  cv_.notify_one();
}

void wait_continuation::wait() {
  std::unique_lock<std::mutex> lock{mutex_};
  cv_.wait(lock, [this]() {return notified_;});
}

bool wait_continuation::wait_for(std::chrono::nanoseconds rel_time) {
  std::unique_lock<std::mutex> lock{mutex_};
  return cv_.wait_for(lock, rel_time, [this]() {return notified_;});
}

} // namespace detail
} // inline namespace cxx14_v1
} // namespace portable_concurrency


