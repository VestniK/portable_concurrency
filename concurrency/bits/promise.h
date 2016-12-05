#pragma once

#include "future.h"
#include "future_error.h"
#include "shared_state.h"

namespace concurrency {

template<typename T>
class promise {
public:
  promise():
    future_state_(std::make_shared<detail::shared_state<T>>()),
    state_(future_state_)
  {}
  promise(promise&&) noexcept = default;
  promise(const promise&) = delete;

  promise& operator= (const promise&) = delete;
  promise& operator= (promise&& rhs) noexcept;

  ~promise() = default;

  future<T> get_future() {
    if (state_.expired())
      throw future_error(future_errc::no_state);
    if (!future_state_)
      throw future_error(future_errc::future_already_retrieved);
    return future<T>{std::move(future_state_)};
  }

  void set_value(const T& val) {
    auto state = state_.lock();
    if (!state)
      throw future_error(future_errc::no_state);
    std::lock_guard<std::mutex>(state->mutex);
    state->box.set_value(val);
    state->cv.notify_all();
  }

  void set_value(T&& val) {
    auto state = state_.lock();
    if (!state)
      throw future_error(future_errc::no_state);
    std::lock_guard<std::mutex>(state->mutex);
    state->box.set_value(std::move(val));
    state->cv.notify_all();
  }

  void set_exception(std::exception_ptr error) {
    auto state = state_.lock();
    if (!state)
      throw future_error(future_errc::no_state);
    std::lock_guard<std::mutex>(state->mutex);
    state->box.set_exception(error);
    state->cv.notify_all();
  }

private:
  std::shared_ptr<detail::shared_state<T>> future_state_;
  std::weak_ptr<detail::shared_state<T>> state_;
};

} // namespace concurrency
