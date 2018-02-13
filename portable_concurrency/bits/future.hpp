#pragma once

#include <future>

#include "future.h"
#include "future_state.h"
#include "then.h"

namespace portable_concurrency {
inline namespace cxx14_v1 {

template<typename T>
shared_future<T> future<T>::share() noexcept {
  return {std::move(*this)};
}

template<typename T>
T future<T>::get() {
  if (!state_)
    throw std::future_error(std::future_errc::no_state);
  wait();
  auto state = std::move(state_);
  return std::move(state->value_ref());
}

template<typename T>
void future<T>::wait() const {
  if (!state_)
    throw std::future_error(std::future_errc::no_state);
  state_->continuations().wait();
}

template<typename T>
template<typename Rep, typename Period>
future_status future<T>::wait_for(const std::chrono::duration<Rep, Period>& rel_time) const {
  if (!state_)
    throw std::future_error(std::future_errc::no_state);
  return state_->continuations().wait_for(std::chrono::duration_cast<std::chrono::nanoseconds>(rel_time)) ?
    future_status::ready:
    future_status::timeout
  ;
}

template<typename T>
template <typename Clock, typename Duration>
future_status future<T>::wait_until(const std::chrono::time_point<Clock, Duration>& abs_time) const {
  return wait_for(abs_time - Clock::now());
}

template<typename T>
bool future<T>::valid() const noexcept {return static_cast<bool>(state_);}

template<typename T>
bool future<T>::is_ready() const {
  if (!state_)
    throw std::future_error(std::future_errc::no_state);
  return state_->continuations().executed();
}

template<typename T>
template<typename F>
detail::cnt_future_t<F, future<T>> future<T>::then(F&& f) {
  if (!state_)
    throw std::future_error(std::future_errc::no_state);
  return detail::make_then_state<detail::cnt_tag::then, T, F>(
    std::move(state_), std::forward<F>(f)
  );
}

template<typename T>
template<typename E, typename F>
auto future<T>::then(E&& exec, F&& f) -> std::enable_if_t<
  is_executor<std::decay_t<E>>::value,
  detail::cnt_future_t<F, future<T>>
> {
  if (!state_)
    throw std::future_error(std::future_errc::no_state);
  return detail::make_then_state<detail::cnt_tag::then, T, E, F>(
    std::move(state_), std::forward<E>(exec), std::forward<F>(f)
  );
}

template<typename T>
template<typename F>
detail::cnt_future_t<F, T> future<T>::next(F&& f) {
  if (!state_)
    throw std::future_error(std::future_errc::no_state);
  return detail::make_then_state<detail::cnt_tag::next, T, F>(std::move(state_), std::forward<F>(f));
}

template<typename T>
template<typename E, typename F>
auto future<T>::next(E&& exec, F&& f) -> std::enable_if_t<
  is_executor<std::decay_t<E>>::value,
  detail::cnt_future_t<F, T>
> {
  if (!state_)
    throw std::future_error(std::future_errc::no_state);
  return detail::make_then_state<detail::cnt_tag::next, T, E, F>(
    std::move(state_), std::forward<E>(exec), std::forward<F>(f)
  );
}

template<typename T>
void future<T>::detach() {
  if (!state_)
    throw std::future_error(std::future_errc::no_state);
  auto state_ref = state_;
  state_ref->push_continuation([captured_state = std::move(state_)]() { });
}

template<typename T>
future<T>::future(std::shared_ptr<detail::future_state<T>>&& state) noexcept:
  state_(std::move(state))
{}

#if defined(__cpp_coroutines)
template<typename T>
bool future<T>::await_ready() const noexcept {return is_ready();}

template<typename T>
T future<T>::await_resume() {return get();}

template<typename T>
void future<T>::await_suspend(std::experimental::coroutine_handle<> handle) {
  state_->push_continuation(std::move(handle));
}
#endif

namespace detail {

template<typename T>
std::shared_ptr<future_state<T>>& state_of(future<T>& f) {
  return f.state_;
}

} // namespace detail

} // inline namespace cxx14_v1
} // namespace portable_concurrency
