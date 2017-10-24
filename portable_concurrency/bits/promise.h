#pragma once

#include <memory>
#include <utility>

#include "future.h"
#include "shared_state.h"
#include "utils.h"

namespace portable_concurrency {
inline namespace cxx14_v1 {
namespace detail {

template<typename T>
class promise_base {
protected:
  std::weak_ptr<shared_state<T>> state_;
  std::shared_ptr<future_state<T>> future_state_;

  void check_state() {
    // the only way to distinguish empty weak_ptr from expired is owner_before method
    std::weak_ptr<shared_state<T>> empty{};
    if (!empty.owner_before(state_) && !state_.owner_before(empty))
      throw std::future_error(std::future_errc::no_state);
  }

  std::shared_ptr<shared_state<T>> get_state() {
    auto state = state_.lock();
    if (!state)
      check_state();
    return state;
  }

public:
  promise_base() {
    auto state = std::make_shared<shared_state<T>>();
    future_state_ = state;
    state_ = std::move(state);
  }
  ~promise_base() {
    auto state = state_.lock();
    if (state && !state->continuations().executed())
      state->set_exception(std::make_exception_ptr(std::future_error{std::future_errc::broken_promise}));
  }

  promise_base(promise_base&&) noexcept = default;
  promise_base& operator= (promise_base&&) noexcept = default;

  future<T> get_future() {
    if (!future_state_)
      check_state(), throw std::future_error(std::future_errc::future_already_retrieved);
    return {std::move(future_state_)};
  }

  void set_exception(std::exception_ptr error) {
    auto state = state_.lock();
    if (!state)
      return check_state();
    state->set_exception(error);
  }
};

} // namespace detail

template<typename T>
class promise: public detail::promise_base<T> {
public:
  promise() = default;
  promise(promise&&) noexcept = default;
  promise(const promise&) = delete;

  promise& operator= (const promise&) = delete;
  promise& operator= (promise&& rhs) noexcept = default;

  ~promise() = default;

  void set_value(const T& val) {
    auto state = this->get_state();
    if (state)
      state->emplace(val);
  }

  void set_value(T&& val) {
    auto state = this->get_state();
    if (state)
      state->emplace(std::move(val));
  }
};

template<typename T>
class promise<T&>: public detail::promise_base<T&> {
public:
  promise() = default;
  promise(promise&&) noexcept = default;
  promise(const promise&) = delete;

  promise& operator= (const promise&) = delete;
  promise& operator= (promise&& rhs) noexcept = default;

  ~promise() = default;

  void set_value(T& val) {
    auto state = this->get_state();
    if (state)
      state->emplace(val);
  }
};

template<>
class promise<void>: public detail::promise_base<void> {
public:
  promise() = default;
  promise(promise&&) noexcept = default;
  promise(const promise&) = delete;

  promise& operator= (const promise&) = delete;
  promise& operator= (promise&& rhs) noexcept = default;

  ~promise() = default;

  void set_value()  {
    auto state = this->get_state();
    if (state)
      state->emplace();
  }
};

} // inline namespace cxx14_v1
} // namespace portable_concurrency
