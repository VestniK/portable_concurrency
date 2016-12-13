#pragma once

#include "future.h"
#include "future_error.h"
#include "shared_state.h"
#include "utils.h"

namespace concurrency {
namespace detail {

template<typename T>
class promise_base {
public:
  future<T> get_future() {
    auto state = state_.lock();
    if (state)
      throw future_error(future_errc::future_already_retrieved);
    state = std::make_shared<detail::shared_state<T>>();
    state_ = state;
    return make_future(std::move(state));
  }

  void set_exception(std::exception_ptr error) {
    auto state = state_.lock();
    if (!state)
      throw future_error(future_errc::no_state);
    state->set_exception(error);
  }

protected:
  std::weak_ptr<detail::shared_state<T>> state_;
};

} // namespace detail

template<typename T>
class promise: public detail::promise_base<T> {
public:
  promise() = default;
  promise(promise&&) noexcept = default;
  promise(const promise&) = delete;

  promise& operator= (const promise&) = delete;
  promise& operator= (promise&& rhs) noexcept;

  ~promise() = default;

  void set_value(const T& val) {
    auto state = this->state_.lock();
    if (!state)
      throw future_error(future_errc::no_state);
    state->emplace(val);
  }

  void set_value(T&& val) {
    auto state = this->state_.lock();
    if (!state)
      throw future_error(future_errc::no_state);
    state->emplace(std::move(val));
  }
};

template<>
class promise<void>: public detail::promise_base<void> {
public:
  promise() = default;
  promise(promise&&) noexcept = default;
  promise(const promise&) = delete;

  promise& operator= (const promise&) = delete;
  promise& operator= (promise&& rhs) noexcept;

  ~promise() = default;

  void set_value() {
    auto state = this->state_.lock();
    if (!state)
      throw future_error(future_errc::no_state);
    state->emplace();
  }
};

} // namespace concurrency
