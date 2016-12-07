#pragma once

#include "future.h"
#include "future_error.h"
#include "shared_state.h"

namespace concurrency {

template<typename T>
class promise {
public:
  promise() = default;
  promise(promise&&) noexcept = default;
  promise(const promise&) = delete;

  promise& operator= (const promise&) = delete;
  promise& operator= (promise&& rhs) noexcept;

  ~promise() = default;

  future<T> get_future() {
    auto state = state_.lock();
    if (state)
      throw future_error(future_errc::future_already_retrieved);
    state = std::make_shared<detail::shared_state<T>>();
    state_ = state;
    return future<T>{std::move(state)};
  }

  void set_value(const T& val) {
    auto state = state_.lock();
    if (!state)
      throw future_error(future_errc::no_state);
    state->set_value(val);
  }

  void set_value(T&& val) {
    auto state = state_.lock();
    if (!state)
      throw future_error(future_errc::no_state);
    state->set_value(std::move(val));
  }

  void set_exception(std::exception_ptr error) {
    auto state = state_.lock();
    if (!state)
      throw future_error(future_errc::no_state);
    state->set_exception(error);
  }

private:
  std::weak_ptr<detail::shared_state<T>> state_;
};

} // namespace concurrency
