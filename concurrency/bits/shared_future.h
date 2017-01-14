#pragma once

#include <future>

#include "fwd.h"

namespace experimental {
inline namespace concurrency_v1 {

template<typename T>
class shared_future {
public:
  shared_future() noexcept = default;
  shared_future(const shared_future&) noexcept = default;
  shared_future(shared_future&&) noexcept = default;
  shared_future(future<T>&& rhs) noexcept:
    state_(std::move(rhs.state_))
  {}

  shared_future& operator=(const shared_future&) noexcept = default;
  shared_future& operator=(shared_future&&) noexcept = default;

  ~shared_future() = default;

  void wait() const {
    if (!state_)
      throw std::future_error(std::future_errc::no_state);
    state_->wait();
  }

  template<typename Rep, typename Period>
  std::future_status wait_for(const std::chrono::duration<Rep, Period>& rel_time) const {
    if (!state_)
      throw std::future_error(std::future_errc::no_state);
    return state_->wait_for(rel_time);
  }

  template <typename Clock, typename Duration>
  std::future_status wait_until(const std::chrono::time_point<Clock, Duration>& abs_time) const {
    if (!state_)
      throw std::future_error(std::future_errc::no_state);
    return state_->wait_until(abs_time);
  }

  bool valid() const noexcept {return static_cast<bool>(state_);}

  decltype(auto) get() {
    if (!state_)
      throw std::future_error(std::future_errc::no_state);
    return state_->shared_get();
  }

  bool is_ready() const {
    if (!state_)
      throw std::future_error(std::future_errc::no_state);
    return state_->is_ready();
  }

private:
  std::shared_ptr<detail::shared_state<T>> state_;
};

} // inline namespace concurrency_v1
} // namespace experimental
