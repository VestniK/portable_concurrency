#pragma once

#include "future_error.h"
#include "future_status.h"
#include "shared_state.h"

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

  // implementation detail
  explicit future(std::shared_ptr<detail::shared_state<T>>&& state) noexcept:
    state_(std::move(state))
  {}

  ~future() = default;

  T get() {
    std::unique_lock<std::mutex> lock(state_->mutex);
    state_->cv.wait(lock, [this] {
      return state_->box.get_state() != detail::box_state::empty;
    });
    auto state = std::move(state_);
    return state->box.get();
  }

  void wait() const {
    std::unique_lock<std::mutex> lock(state_->mutex);
    state_->cv.wait(lock, [this] {
      return state_->box.get_state() != detail::box_state::empty;
    });
  }

  template<typename Rep, typename Period>
  future_status wait_for(const std::chrono::duration<Rep, Period>& rel_time) const {
    std::unique_lock<std::mutex> lock(state_->mutex);
    const bool wait_res = state_->cv.wait_for(lock, rel_time, [this] {
      return state_->box.get_state() != detail::box_state::empty;
    });
    return wait_res ? future_status::ready : future_status::timeout;
  }

  template <class Clock, class Duration>
  future_status wait_until(const std::chrono::time_point<Clock, Duration>& abs_time) const {
    std::unique_lock<std::mutex> lock(state_->mutex);
    const bool wait_res = state_->cv.wait_until(lock, abs_time, [this] {
      return state_->box.get_state() != detail::box_state::empty;
    });
    return wait_res ? future_status::ready : future_status::timeout;
  }

  bool valid() const {return static_cast<bool>(state_);}

private:
  std::shared_ptr<detail::shared_state<T>> state_;
};

} // namespace concurrency
