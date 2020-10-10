#pragma once

#include "execution.h"
#include "future.h"
#include "future_state.h"
#include "shared_future.h"
#include "then.hpp"
#if !defined(PC_NO_DEPRECATED)
#include "timed_waiter.h"
#endif
#include "utils.h"

namespace portable_concurrency {
inline namespace cxx14_v1 {

template <typename T>
shared_future<T>::shared_future(future<T>&& rhs) noexcept : state_(std::move(rhs.state_)) {}

template <typename T>
void shared_future<T>::wait() const {
  if (!state_)
    detail::throw_no_state();
  detail::wait(*state_);
}

#if !defined(PC_NO_DEPRECATED)
template <typename T>
template <typename Rep, typename Period>
future_status shared_future<T>::wait_for(const std::chrono::duration<Rep, Period>& rel_time) const {
  if (!state_)
    detail::throw_no_state();
  // const_cast below:
  //  * can't introduce thread safety issues since adding notification is thread safe
  //  * can't lead to UB since the only object which s modified is shared_state which is always non const object
  //    even if future owning it is const.
  // This implementation is provided for compatibility only and will be removed in future versions. No stinky const
  // cast are going to survive.
  timed_waiter waiter{const_cast<shared_future<T>&>(*this)};
  return waiter.wait_for(rel_time);
}

template <typename T>
template <typename Clock, typename Duration>
future_status shared_future<T>::wait_until(const std::chrono::time_point<Clock, Duration>& abs_time) const {
  if (!state_)
    detail::throw_no_state();
  // const_cast below:
  //  * can't introduce thread safety issues since adding notification is thread safe
  //  * can't lead to UB since the only object which s modified is shared_state which is always non const object
  //    even if future owning it is const.
  // This implementation is provided for compatibility only and will be removed in future versions. No stinky const
  // cast are going to survive.
  timed_waiter waiter{const_cast<shared_future<T>&>(*this)};
  return waiter.wait_until(abs_time);
}
#endif

template <typename T>
bool shared_future<T>::valid() const noexcept {
  return static_cast<bool>(state_);
}

template <typename T>
typename shared_future<T>::get_result_type shared_future<T>::get() const {
  if (!state_)
    detail::throw_no_state();
  wait();
  return state_->value_ref();
}

template <typename T>
bool shared_future<T>::is_ready() const {
  if (!state_)
    detail::throw_no_state();
  return state_->continuations().executed();
}

template <typename T>
template <typename F>
void shared_future<T>::notify(F&& notification) {
  if (!state_)
    detail::throw_no_state();
  state_->continuations().push(std::forward<F>(notification));
}

template <typename T>
template <typename E, typename F>
void shared_future<T>::notify(E&& exec, F&& notification) {
  static_assert(is_executor<std::decay_t<E>>::value, "E must be an executor");
  if (!state_)
    detail::throw_no_state();
  state_->continuations().push([exec = std::forward<E>(exec), notification = std::forward<F>(notification)]() mutable {
    post(exec, std::move(notification));
  });
}

template <typename T>
template <typename F>
PC_NODISCARD detail::cnt_future_t<F, shared_future<T>> shared_future<T>::then(F&& f) const {
  return then(inplace_executor, std::forward<F>(f));
}

template <typename T>
template <typename E, typename F>
PC_NODISCARD detail::cnt_future_t<F, shared_future<T>> shared_future<T>::then(E&& exec, F&& f) const {
  static_assert(is_executor<std::decay_t<E>>::value, "E must be an executor");
  using result_type = detail::remove_future_t<detail::cnt_result_t<F, shared_future<T>>>;
  if (!state_)
    detail::throw_no_state();
  return detail::make_then_state<result_type>(state_->continuations(), std::forward<E>(exec),
      detail::decorate_shared_then<result_type, T, F>(std::forward<F>(f), state_));
}

template <typename T>
template <typename F>
PC_NODISCARD detail::cnt_future_t<F, typename shared_future<T>::get_result_type> shared_future<T>::next(F&& f) const {
  return next(inplace_executor, std::forward<F>(f));
}

template <typename T>
template <typename E, typename F>
PC_NODISCARD detail::cnt_future_t<F, typename shared_future<T>::get_result_type> shared_future<T>::next(
    E&& exec, F&& f) const {
  static_assert(is_executor<std::decay_t<E>>::value, "E must be an executor");
  using result_type = detail::remove_future_t<detail::cnt_result_t<F, typename shared_future<T>::get_result_type>>;
  if (!state_)
    detail::throw_no_state();
  return detail::make_then_state<result_type>(state_->continuations(), std::forward<E>(exec),
      detail::decorate_shared_next<result_type, T, F>(std::forward<F>(f), state_));
}

template <>
template <typename E, typename F>
PC_NODISCARD detail::cnt_future_t<F, typename shared_future<void>::get_result_type> shared_future<void>::next(
    E&& exec, F&& f) const {
  static_assert(is_executor<std::decay_t<E>>::value, "E must be an executor");
  using result_type = detail::remove_future_t<detail::cnt_result_t<F, void>>;
  if (!state_)
    detail::throw_no_state();
  return detail::make_then_state<result_type>(state_->continuations(), std::forward<E>(exec),
      detail::decorate_void_next<result_type, F>(std::forward<F>(f), state_));
}

template <typename T>
template <typename F>
PC_NODISCARD detail::add_future_t<detail::promise_arg_t<F, shared_future<T>>> shared_future<T>::then(F&& f) {
  return then(inplace_executor, std::forward<F>(f));
}

/**
 * Attaches interruptible continuation function `f` to this shared_future object. [EXTENSION]
 *
 * Function must be callable with signature `void(promise<R>, shared_future<T>)`. Promise object passed as the first
 * parameter can be used
 *  * to test if the result of current operation is awaited by any future or shared_future with `promise::is_awaiten`
 *    member function
 *  * to set the result or failure of the current operation with `promise::set_value` or `promise::set_exception` member
 *    functions
 *  * to move it into another promise which will be used to set value or exception by some other operation
 *
 * If continuation function exits via exception std::terminate is called
 */
template <typename T>
template <typename E, typename F>
PC_NODISCARD detail::add_future_t<detail::promise_arg_t<F, shared_future<T>>> shared_future<T>::then(E&& exec, F&& f) {
  static_assert(is_executor<std::decay_t<E>>::value, "E must be an executor");
  using result_type = detail::promise_arg_t<F, shared_future<T>>;
  if (!state_)
    detail::throw_no_state();
  detail::continuations_stack& subscriptions = state_->continuations();
  return detail::make_then_state<result_type>(subscriptions, std::forward<E>(exec),
      [f = std::forward<F>(f), parent = std::move(state_)](
          std::shared_ptr<detail::shared_state<result_type>> state) mutable noexcept {
        promise<result_type> p{std::exchange(state, nullptr)};
        ::portable_concurrency::detail::invoke(f, std::move(p), shared_future<T>{std::move(parent)});
      });
}

template <typename T>
shared_future<T> shared_future<T>::detach() {
  if (!state_)
    detail::throw_no_state();
  auto& state_ref = *state_;
  state_ref.push([captured_state = state_] {});
  return std::move(*this);
}

template <typename T>
shared_future<T>::shared_future(std::shared_ptr<detail::future_state<T>>&& state) noexcept : state_(std::move(state)) {}

#if defined(__cpp_coroutines)
template <typename T>
bool shared_future<T>::await_ready() const noexcept {
  return is_ready();
}

template <typename T>
typename shared_future<T>::get_result_type shared_future<T>::await_resume() const {
  return get();
}

template <typename T>
void shared_future<T>::await_suspend(std::experimental::coroutine_handle<> handle) const {
  state_->push(std::move(handle));
}
#endif

namespace detail {

template <typename T>
std::shared_ptr<future_state<T>>& state_of(shared_future<T>& f) {
  return f.state_;
}

template <typename T>
std::shared_ptr<future_state<T>> state_of(shared_future<T>&& f) {
  return f.state_;
}

} // namespace detail

} // namespace cxx14_v1
} // namespace portable_concurrency
