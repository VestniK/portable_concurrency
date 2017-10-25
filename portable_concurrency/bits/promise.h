#pragma once

#include <memory>
#include <utility>

#include "either.h"
#include "future.h"
#include "shared_state.h"
#include "utils.h"

namespace portable_concurrency {
inline namespace cxx14_v1 {
namespace detail {

template<typename T>
struct promise_common {
  either<std::shared_ptr<shared_state<T>>, std::weak_ptr<shared_state<T>>> state_{
    first_t{}, std::make_shared<shared_state<T>>()
  };

  promise_common() = default;
  ~promise_common() {
    auto state = get_state(false);
    if (state && !state->continuations().executed())
      state->set_exception(std::make_exception_ptr(std::future_error{std::future_errc::broken_promise}));
  }

  promise_common(promise_common&&) noexcept = default;
  promise_common& operator= (promise_common&&) noexcept = default;

  future<T> get_future() {
    if (state_.state() == either_state::second)
      throw std::future_error(std::future_errc::future_already_retrieved);
    auto state = get_state();
    state_.emplace(second_t{}, state);
    return {state};
  }

  void set_exception(std::exception_ptr error) {
    auto state = get_state();
    if (state)
      state->set_exception(error);
  }

  std::shared_ptr<shared_state<T>> get_state(bool throw_no_state = true) {
    switch (state_.state())
    {
    case either_state::first: return state_.get(first_t{});
    case either_state::second: return state_.get(second_t{}).lock();
    case either_state::empty: break;
    }
    if (throw_no_state)
      throw std::future_error(std::future_errc::no_state);
    return nullptr;
  }
};

} // namespace detail

template<typename T>
class promise {
public:
  promise() = default;
  promise(promise&&) noexcept = default;
  promise(const promise&) = delete;

  promise& operator= (const promise&) = delete;
  promise& operator= (promise&& rhs) noexcept = default;

  ~promise() = default;

  void set_value(const T& val) {
    auto state = common_.get_state();
    if (state)
      state->emplace(val);
  }

  void set_value(T&& val) {
    auto state = common_.get_state();
    if (state)
      state->emplace(std::move(val));
  }

  future<T> get_future() {return common_.get_future();}
  void set_exception(std::exception_ptr error) {common_.set_exception(error);}

private:
  detail::promise_common<T> common_;
};

template<typename T>
class promise<T&> {
public:
  promise() = default;
  promise(promise&&) noexcept = default;
  promise(const promise&) = delete;

  promise& operator= (const promise&) = delete;
  promise& operator= (promise&& rhs) noexcept = default;

  ~promise() = default;

  void set_value(T& val) {
    auto state = common_.get_state();
    if (state)
      state->emplace(val);
  }

  future<T&> get_future() {return common_.get_future();}
  void set_exception(std::exception_ptr error) {common_.set_exception(error);}

private:
  detail::promise_common<T&> common_;
};

template<>
class promise<void> {
public:
  promise() = default;
  promise(promise&&) noexcept = default;
  promise(const promise&) = delete;

  promise& operator= (const promise&) = delete;
  promise& operator= (promise&& rhs) noexcept = default;

  ~promise() = default;

  void set_value()  {
    auto state = common_.get_state();
    if (state)
      state->emplace();
  }

  future<void> get_future() {return common_.get_future();}
  void set_exception(std::exception_ptr error) {common_.set_exception(error);}

private:
  detail::promise_common<void> common_;
};

} // inline namespace cxx14_v1
} // namespace portable_concurrency
