#pragma once

#include "execution.h"
#include "future.h"
#include "future_state.h"
#include "then.hpp"
#if !defined(PC_NO_DEPRECATED)
#include "timed_waiter.h"
#endif
#include "utils.h"

namespace portable_concurrency {
inline namespace cxx14_v1 {

template <typename T>
shared_future<T> future<T>::share() noexcept {
  return {std::move(*this)};
}

template <typename T>
T future<T>::get() {
  if (!state_)
    detail::throw_no_state();
  wait();
  auto state = std::move(state_);
  return std::move(state->value_ref());
}

template <typename T>
void future<T>::wait() const {
  if (!state_)
    detail::throw_no_state();
  detail::wait(*state_);
}

#if !defined(PC_NO_DEPRECATED)
template <typename T>
template <typename Rep, typename Period>
future_status future<T>::wait_for(const std::chrono::duration<Rep, Period>& rel_time) const {
  if (!state_)
    detail::throw_no_state();
  // const_cast below:
  //  * can't introduce thread safety issues since adding notification is thread safe
  //  * can't lead to UB since the only object which s modified is shared_state which is always non const object
  //    even if future owning it is const.
  // This implementation is provided for compatibility only and will be removed in future versions. No stinky const
  // cast are going to survive.
  timed_waiter waiter{const_cast<future<T>&>(*this)};
  return waiter.wait_for(rel_time);
}

template <typename T>
template <typename Clock, typename Duration>
future_status future<T>::wait_until(const std::chrono::time_point<Clock, Duration>& abs_time) const {
  if (!state_)
    detail::throw_no_state();
  // const_cast below:
  //  * can't introduce thread safety issues since adding notification is thread safe
  //  * can't lead to UB since the only object which s modified is shared_state which is always non const object
  //    even if future owning it is const.
  // This implementation is provided for compatibility only and will be removed in future versions. No stinky const
  // cast are going to survive.
  timed_waiter waiter{const_cast<future<T>&>(*this)};
  return waiter.wait_until(abs_time);
}
#endif

template <typename T>
bool future<T>::valid() const noexcept {
  return static_cast<bool>(state_);
}

template <typename T>
bool future<T>::is_ready() const {
  if (!state_)
    detail::throw_no_state();
  return state_->continuations().executed();
}

template <typename T>
template <typename F>
void future<T>::notify(F&& notification) {
  if (!state_)
    detail::throw_no_state();
  state_->continuations().push(std::forward<F>(notification));
}

template <typename T>
template <typename E, typename F>
void future<T>::notify(E&& exec, F&& notification) {
  static_assert(is_executor<std::decay_t<E>>::value, "E must be an executor");
  if (!state_)
    detail::throw_no_state();
  state_->continuations().push([exec = std::forward<E>(exec), notification = std::forward<F>(notification)]() mutable {
    post(exec, std::move(notification));
  });
}

template <typename T>
template <typename F>
PC_NODISCARD detail::cnt_future_t<F, future<T>> future<T>::then(F&& f) {
  return then(inplace_executor, std::forward<F>(f));
}

template <typename T>
template <typename F>
PC_NODISCARD detail::add_future_t<detail::promise_arg_t<F, future<T>>> future<T>::then(F&& f) {
  return then(inplace_executor, std::forward<F>(f));
}

template <typename T>
template <typename E, typename F>
PC_NODISCARD detail::cnt_future_t<F, future<T>> future<T>::then(E&& exec, F&& f) {
  static_assert(is_executor<std::decay_t<E>>::value, "E must be an executor");
  using result_type = detail::remove_future_t<detail::cnt_result_t<F, future<T>>>;
  if (!state_)
    detail::throw_no_state();
  detail::continuations_stack& subscriptions = state_->continuations();
  return detail::make_then_state<result_type>(subscriptions, std::forward<E>(exec),
      detail::decorate_unique_then<result_type, T, F>(std::forward<F>(f), std::move(state_)));
}

/**
 * Attaches interruptable continuation function `f` to this future object. [EXTENSION]
 *
 * Function must be callable with signature `void(promise<R>, future<T>)`. Promise object passed as the first parameter
 * can be used
 *  * to test if the result of current operation is awaiten by any future or shared_future with `promise::is_awaiten`
 *    member function
 *  * to set the result or failure of the current operation with `promise::set_value` or `promise::set_exception` member
 *    functions
 *  * to move it into another promise which will be used to set vale or exception by some other operation
 *
 * If continuation function exits via exception std::terminate is called.
 */
template <typename T>
template <typename E, typename F>
PC_NODISCARD detail::add_future_t<detail::promise_arg_t<F, future<T>>> future<T>::then(E&& exec, F&& f) {
  static_assert(is_executor<std::decay_t<E>>::value, "E must be an executor");
  using result_type = detail::promise_arg_t<F, future<T>>;
  if (!state_)
    detail::throw_no_state();
  detail::continuations_stack& subscriptions = state_->continuations();
  return detail::make_then_state<result_type>(subscriptions, std::forward<E>(exec),
      [f = std::forward<F>(f), parent = std::move(state_)](
          std::shared_ptr<detail::shared_state<result_type>> state) mutable noexcept {
        promise<result_type> p{std::exchange(state, nullptr)};
        ::portable_concurrency::detail::invoke(f, std::move(p), future<T>{std::move(parent)});
      });
}

template <typename T>
template <typename F>
PC_NODISCARD detail::cnt_future_t<F, T> future<T>::next(F&& f) {
  return next(inplace_executor, std::forward<F>(f));
}

template <>
template <typename E, typename F>
PC_NODISCARD detail::cnt_future_t<F, void> future<void>::next(E&& exec, F&& f) {
  static_assert(is_executor<std::decay_t<E>>::value, "E must be an executor");
  using result_type = detail::remove_future_t<detail::cnt_result_t<F, void>>;
  if (!state_)
    detail::throw_no_state();
  detail::continuations_stack& subscriptions = state_->continuations();
  return detail::make_then_state<result_type>(subscriptions, std::forward<E>(exec),
      detail::decorate_void_next<result_type, F>(std::forward<F>(f), std::move(state_)));
}

template <typename T>
template <typename E, typename F>
PC_NODISCARD detail::cnt_future_t<F, T> future<T>::next(E&& exec, F&& f) {
  static_assert(is_executor<std::decay_t<E>>::value, "E must be an executor");
  using result_type = detail::remove_future_t<detail::cnt_result_t<F, T>>;
  if (!state_)
    detail::throw_no_state();
  detail::continuations_stack& subscriptions = state_->continuations();
  return detail::make_then_state<result_type>(subscriptions, std::forward<E>(exec),
      detail::decorate_unique_next<result_type, T, F>(std::forward<F>(f), std::move(state_)));
}

template <typename T>
future<T> future<T>::detach() {
  if (!state_)
    detail::throw_no_state();
  auto& state_ref = *state_;
  state_ref.push([captured_state = state_] {});
  return std::move(*this);
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
