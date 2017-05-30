#pragma once

#include <memory>
#include <utility>

#include "future.h"
#include "shared_state.h"
#include "utils.h"

namespace experimental {
inline namespace concurrency_v1 {
namespace detail {

template<typename T>
class promise_base {
protected:
  // TODO: find the way to avoid enable_shared_from_this in the future<future<T>>
  // and use aggregation with aliasing shared_ptr constructor instead of deriving from
  // shared_state
  struct promise_state: public shared_state<T> {
    bool future_retreived = false;
  };
  std::shared_ptr<promise_state> state_ = std::make_shared<promise_state>();

public:
  future<T> get_future() {
    if (!state_)
      throw std::future_error(std::future_errc::no_state);
    if (std::exchange(state_->future_retreived, true) != false)
      throw std::future_error(std::future_errc::future_already_retrieved);
    return {state_};
  }

  void set_exception(std::exception_ptr error) {
    if (!state_)
      throw std::future_error(std::future_errc::no_state);
    state_->set_exception(error);
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
    this->state_->emplace(val);
  }

  void set_value(T&& val) {
    if (!this->state_)
      throw std::future_error(std::future_errc::no_state);
    this->state_->emplace(std::move(val));
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
    this->state_->emplace(val);
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

  void set_value() {
    if (!this->state_)
      throw std::future_error(std::future_errc::no_state);
    this->state_->emplace();
  }
};

} // inline namespace concurrency_v1
} // namespace experimental
