#pragma once

#include <chrono>
#include <condition_variable>
#include <mutex>

#include "continuation.h"

namespace portable_concurrency {
inline namespace cxx14_v1 {
namespace detail {

class wait_continuation final: public detail::continuation {
public:
  wait_continuation() = default;

  void invoke(const std::shared_ptr<continuation>&) override;
  void wait();
  bool wait_for(std::chrono::nanoseconds rel_time);

private:
  std::mutex mutex_;
  std::condition_variable cv_;
  bool notified_ = false;
};

} // namespace detail
} // inline namespace cxx14_v1
} // namespace portable_concurrency
