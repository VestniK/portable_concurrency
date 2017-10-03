#pragma once

#include <future>
#include <type_traits>

#include "fwd.h"

#include "concurrency_type_traits.h"
#include "shared_state.h"
#include "utils.h"

namespace portable_concurrency {
inline namespace cxx14_v1 {

namespace detail {

template<typename T>
std::shared_ptr<future_state<T>>& state_of(future<T>&);

} // namespace detail

template<typename T>
class future {
public:
  future() noexcept = default;
  future(future&&) noexcept = default;
  future(const future&) = delete;

  future(future<future<T>>&& wrapped) noexcept;

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
    return state_->continuations().is_consumed();
  }

  template<typename F>
  future<detail::continuation_result_t<portable_concurrency::cxx14_v1::future, F, T>>
  then(F&& f) {
    using R = detail::continuation_result_t<portable_concurrency::cxx14_v1::future, F, T>;
    if (!state_)
      throw std::future_error(std::future_errc::no_state);
    struct cnt_data_t {
      std::decay_t<F> func;
      std::shared_ptr<detail::future_state<T>> parent;
      detail::shared_state<R> state;

      cnt_data_t(F&& func, std::shared_ptr<detail::future_state<T>>&& parent):
        func(std::forward<F>(func)), parent(std::move(parent)) {}
    };
    auto cnt_data = std::make_shared<cnt_data_t>(
      std::forward<F>(f), std::move(state_)
    );
    cnt_data->parent->continuations().push([cnt_data]{
      detail::set_state_value(
        std::shared_ptr<detail::shared_state<R>>(cnt_data, &cnt_data->state),
        std::move(cnt_data->func), future{std::move(cnt_data->parent)}
      );
    });
    return {std::shared_ptr<detail::future_state<R>>(cnt_data, &cnt_data->state)};
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
