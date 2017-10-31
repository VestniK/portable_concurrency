#pragma once

#include <future>
#include <type_traits>

#include "fwd.h"

#include "concurrency_type_traits.h"
#include "execution.h"
#include "future_state.h"
#include "then.h"

namespace portable_concurrency {
inline namespace cxx14_v1 {

template<typename T>
class future {
  static_assert(!detail::is_future<T>::value, "future<future<T>> and future<shared_future<T>> are not allowed");
public:
  future() noexcept = default;
  future(future&&) noexcept = default;
  future(const future&) = delete;

  future& operator= (const future&) = delete;
  future& operator= (future&& rhs) noexcept = default;

  ~future() = default;

  shared_future<T> share() noexcept {
    return {std::move(*this)};
  }

  T get() {
    if (!state_)
      throw std::future_error(std::future_errc::no_state);
    wait();
    auto state = std::move(state_);
    return std::move(state->value_ref());
  }

  void wait() const {
    if (!state_)
      throw std::future_error(std::future_errc::no_state);
    state_->continuations().wait();
  }

  template<typename Rep, typename Period>
  std::future_status wait_for(const std::chrono::duration<Rep, Period>& rel_time) const {
    if (!state_)
      throw std::future_error(std::future_errc::no_state);
    return state_->continuations().wait_for(std::chrono::duration_cast<std::chrono::nanoseconds>(rel_time)) ?
      std::future_status::ready:
      std::future_status::timeout
    ;
  }

  template <typename Clock, typename Duration>
  std::future_status wait_until(const std::chrono::time_point<Clock, Duration>& abs_time) const {
    return wait_for(abs_time - Clock::now());
  }

  bool valid() const noexcept {return static_cast<bool>(state_);}

  bool is_ready() const {
    if (!state_)
      throw std::future_error(std::future_errc::no_state);
    return state_->continuations().executed();
  }

  template<typename F>
  detail::cnt_future_t<F, future<T>>
  then(F&& f) {
    if (!state_)
      throw std::future_error(std::future_errc::no_state);
    return detail::make_then_state<T, F, future<T>>(
      std::move(state_), std::forward<F>(f)
    );
  }

  template<typename E, typename F>
  auto then(E&& exec, F&& f) -> std::enable_if_t<
    is_executor<std::decay_t<E>>::value,
    detail::cnt_future_t<F, future<T>>
  > {
    if (!state_)
      throw std::future_error(std::future_errc::no_state);
    return detail::make_then_state<T, E, F, future<T>>(
      std::move(state_), std::forward<E>(exec), std::forward<F>(f)
    );
  }

  template<typename F>
  detail::cnt_future_t<F, T> next(F&& f) {
    if (!state_)
      throw std::future_error(std::future_errc::no_state);
    return detail::make_next_state<T, F>(std::move(state_), std::forward<F>(f));
  }

  template<typename E, typename F>
  auto next(E&& exec, F&& f) -> std::enable_if_t<
    is_executor<std::decay_t<E>>::value,
    detail::cnt_future_t<F, T>
  > {
    if (!state_)
      throw std::future_error(std::future_errc::no_state);
    return detail::make_next_state<T, E, F>(
      std::move(state_), std::forward<E>(exec), std::forward<F>(f)
    );
  }

  // implementation detail
  future(std::shared_ptr<detail::future_state<T>>&& state) noexcept:
    state_(std::move(state))
  {}

private:
  friend class shared_future<T>;
  friend std::shared_ptr<detail::future_state<T>>& detail::state_of<T>(future<T>&);

private:
  std::shared_ptr<detail::future_state<T>> state_;
};

template<>
inline
void future<void>::get() {
  if (!state_)
    throw std::future_error(std::future_errc::no_state);
  wait();
  auto state = std::move(state_);
  state->value_ref();
}

namespace detail {

template<typename T>
std::shared_ptr<future_state<T>>& state_of(future<T>& f) {
  return f.state_;
}

} // namespace detail

} // inline namespace cxx14_v1
} // namespace portable_concurrency
