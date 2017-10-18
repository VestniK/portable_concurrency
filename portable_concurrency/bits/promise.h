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
  struct promise_state {
    shared_state<T> state;
    bool future_retreived = false;
  };
  std::shared_ptr<promise_state> state_ = std::make_shared<promise_state>();

  std::shared_ptr<shared_state<T>> get_state_ptr() {return {state_, &state_->state};}

public:
  future<T> get_future() {
    if (!state_)
      throw std::future_error(std::future_errc::no_state);
    if (std::exchange(state_->future_retreived, true) != false)
      throw std::future_error(std::future_errc::future_already_retrieved);
    return {get_state_ptr()};
  }

  void set_exception(std::exception_ptr error) {
    if (!state_)
      throw std::future_error(std::future_errc::no_state);
    state_->state.set_exception(error);
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
  promise& operator= (promise&& rhs) noexcept;

  ~promise() = default;

  void set_value(const T& val) {
    if (!this->state_)
      throw std::future_error(std::future_errc::no_state);
    this->state_->state.emplace(val);
  }

  void set_value(T&& val) {
    if (!this->state_)
      throw std::future_error(std::future_errc::no_state);
    this->state_->state.emplace(std::move(val));
  }
};

template<typename T>
class promise<T&>: public detail::promise_base<T&> {
public:
  promise() = default;
  promise(promise&&) noexcept = default;
  promise(const promise&) = delete;

  promise& operator= (const promise&) = delete;
  promise& operator= (promise&& rhs) noexcept;

  ~promise() = default;

  void set_value(T& val) {
    if (!this->state_)
      throw std::future_error(std::future_errc::no_state);
    this->state_->state.emplace(val);
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

  void set_value();
};

} // inline namespace cxx14_v1
} // namespace portable_concurrency
