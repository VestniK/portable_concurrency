#pragma once

#include <future>

#include "fwd.h"

#include "shared_state.h"
#include "utils.h"

namespace concurrency {

template<typename T>
class future {
public:
  future() noexcept = default;
  future(future&&) noexcept = default;
  future(const future&) = delete;

  future& operator= (const future&) = delete;
  future& operator= (future&& rhs) noexcept {
    state_ = std::move(rhs.state_);
    rhs.state_ = nullptr;
    return *this;
  }

  ~future() = default;

  shared_future<T> share() noexcept {
    return {std::move(*this)};
  }

  T get() {
    if (!state_)
      throw std::future_error(std::future_errc::no_state);
    auto state = std::move(state_);
    return state->get();
  }

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

  bool is_ready() const {
    if (!state_)
      throw std::future_error(std::future_errc::no_state);
    return state_->is_ready();
  }

private:
  friend class shared_future<T>;
  friend future<T> detail::make_future<T>(std::shared_ptr<detail::shared_state<T>>&& state);

  explicit future(std::shared_ptr<detail::shared_state<T>>&& state) noexcept:
    state_(std::move(state))
  {}

private:
  std::shared_ptr<detail::shared_state<T>> state_;
};

} // namespace concurrency
