#pragma once

#include <future>

#include "execution.h"
#include "future.h"
#include "future_state.h"
#include "then.hpp"

namespace portable_concurrency {
inline namespace cxx14_v1 {

template <typename T>
shared_future<T> future<T>::share() noexcept {
  return {std::move(*this)};
}

template <typename T>
T future<T>::get() {
  if (!state_)
    throw std::future_error(std::future_errc::no_state);
  wait();
  auto state = std::move(state_);
  return std::move(state->value_ref());
}

template <typename T>
void future<T>::wait() const {
  if (!state_)
    throw std::future_error(std::future_errc::no_state);
  auto& continuations = state_->continuations();
  if (!continuations.executed())
    continuations.get_waiter().wait();
}

template <typename T>
template <typename Rep, typename Period>
future_status future<T>::wait_for(const std::chrono::duration<Rep, Period>& rel_time) const {
  if (!state_)
    throw std::future_error(std::future_errc::no_state);
  auto& continuations = state_->continuations();
  if (continuations.executed())
    return future_status::ready;
  return continuations.get_waiter().wait_for(std::chrono::duration_cast<std::chrono::nanoseconds>(rel_time))
             ? future_status::ready
             : future_status::timeout;
}

template <typename T>
template <typename Clock, typename Duration>
future_status future<T>::wait_until(const std::chrono::time_point<Clock, Duration>& abs_time) const {
  return wait_for(abs_time - Clock::now());
}

template <typename T>
bool future<T>::valid() const noexcept {
  return static_cast<bool>(state_);
}

template <typename T>
bool future<T>::is_ready() const {
  if (!state_)
    throw std::future_error(std::future_errc::no_state);
  return state_->continuations().executed();
}

template <typename T>
template <typename F>
PC_NODISCARD
detail::cnt_future_t<F, future<T>> future<T>::then(F&& f) {
  return then(detail::inplace_executor{}, std::forward<F>(f));
}

template <typename T>
template <typename F>
PC_NODISCARD
detail::add_future_t<detail::promise_arg_t<F, T>> future<T>::then(F&& f) {
  return then(detail::inplace_executor{}, std::forward<F>(f));
}

template <typename T>
template <typename E, typename F>
PC_NODISCARD
detail::cnt_future_t<F, future<T>> future<T>::then(E&& exec, F&& f) {
  static_assert(is_executor<std::decay_t<E>>::value, "E must be an executor");
  using result_type = detail::remove_future_t<detail::cnt_result_t<F, future<T>>>;
  if (!state_)
    throw std::future_error(std::future_errc::no_state);
  detail::continuations_stack& subscriptions = state_->continuations();
  return detail::make_then_state<result_type>(subscriptions, std::forward<E>(exec),
      detail::decorate_unique_then<result_type, T, F>(std::forward<F>(f), std::move(state_)));
}

/**
 * Attaches interruptable continuation function `f` to this future object. [EXTENSION]
 *
 * Function must be callable with signature `void(promise<R>&, future<T>)`. Promise object passed as the first parameter
 * can be used
 *  * to test if the result of current operation is awaiten by any future or shared_future with `promise::is_awaiten`
 *    member function
 *  * to set the result or failure of the current operation with `promise::set_value` or `promise::set_exception` member
 *    functions
 *  * to move it into another promise which will be used to set vale or exception by some other operation
 *
 * If continuation function exits via exception it will be caut and stored in a shared state assotiated with the
 * continuation as if `promise::set_exception` is called on a promise object passed as the first argume ot `f`.
 * Note: This means that the behavior is undefined if continuation function moved promise argument to some other promise
 * object and then thrown an exception.
 */
template <typename T>
template <typename E, typename F>
PC_NODISCARD
detail::add_future_t<detail::promise_arg_t<F, T>> future<T>::then(E&& exec, F&& f) {
  static_assert(is_executor<std::decay_t<E>>::value, "E must be an executor");
  using result_type = detail::promise_arg_t<F, T>;
  if (!state_)
    throw std::future_error(std::future_errc::no_state);
  detail::continuations_stack& subscriptions = state_->continuations();
  return detail::make_then_state<result_type>(subscriptions, std::forward<E>(exec),
      [f = std::forward<F>(f), parent = std::move(state_)](
          std::shared_ptr<detail::shared_state<result_type>> state) mutable {
        promise<result_type> p{std::exchange(state, nullptr)};
        try {
          ::portable_concurrency::detail::invoke(f, p, future<T>{std::move(parent)});
        } catch (...) {
          p.set_exception(std::current_exception());
        }
      });
}

template <typename T>
template <typename F>
PC_NODISCARD
detail::cnt_future_t<F, T> future<T>::next(F&& f) {
  return next(detail::inplace_executor{}, std::forward<F>(f));
}

template <>
template <typename E, typename F>
PC_NODISCARD
detail::cnt_future_t<F, void> future<void>::next(E&& exec, F&& f) {
  static_assert(is_executor<std::decay_t<E>>::value, "E must be an executor");
  using result_type = detail::remove_future_t<detail::cnt_result_t<F, void>>;
  if (!state_)
    throw std::future_error(std::future_errc::no_state);
  detail::continuations_stack& subscriptions = state_->continuations();
  return detail::make_then_state<result_type>(subscriptions, std::forward<E>(exec),
      detail::decorate_void_next<result_type, F>(std::forward<F>(f), std::move(state_)));
}

template <typename T>
template <typename E, typename F>
PC_NODISCARD
detail::cnt_future_t<F, T> future<T>::next(E&& exec, F&& f) {
  static_assert(is_executor<std::decay_t<E>>::value, "E must be an executor");
  using result_type = detail::remove_future_t<detail::cnt_result_t<F, T>>;
  if (!state_)
    throw std::future_error(std::future_errc::no_state);
  detail::continuations_stack& subscriptions = state_->continuations();
  return detail::make_then_state<result_type>(subscriptions, std::forward<E>(exec),
      detail::decorate_unique_next<result_type, T, F>(std::forward<F>(f), std::move(state_)));
}

template <typename T>
void future<T>::detach() {
  if (!state_)
    throw std::future_error(std::future_errc::no_state);
  auto* state_ref = state_.get();
  state_ref->push([captured_state = std::move(state_)] {});
}

template <typename T>
future<T>::future(std::shared_ptr<detail::future_state<T>>&& state) noexcept : state_(std::move(state)) {}

#if defined(__cpp_coroutines)
template <typename T>
bool future<T>::await_ready() const noexcept {
  return is_ready();
}

template <typename T>
T future<T>::await_resume() {
  return get();
}

template <typename T>
void future<T>::await_suspend(std::experimental::coroutine_handle<> handle) {
  state_->push(std::move(handle));
}
#endif

namespace detail {

template <typename T>
std::shared_ptr<future_state<T>>& state_of(future<T>& f) {
  return f.state_;
}

template <typename T>
std::shared_ptr<future_state<T>> state_of(future<T>&& f) {
  return f.state_;
}

} // namespace detail

} // namespace cxx14_v1
} // namespace portable_concurrency
