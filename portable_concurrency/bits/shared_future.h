#pragma once

#include <future>

#include "fwd.h"

#include "concurrency_type_traits.h"
#include "shared_state.h"
#include "then.h"
#include "utils.h"

namespace portable_concurrency {
inline namespace cxx14_v1 {

namespace detail {

template<typename T>
future_state<T>* state_of(shared_future<T>&);

} // namespace detail

template<typename T>
class shared_future {
public:
  shared_future() noexcept = default;
  shared_future(const shared_future&) noexcept = default;
  shared_future(shared_future&&) noexcept = default;
  shared_future(future<T>&& rhs) noexcept:
    state_(std::move(rhs.state_))
  {}

  shared_future& operator=(const shared_future&) noexcept = default;
  shared_future& operator=(shared_future&&) noexcept = default;

  ~shared_future() = default;

  void wait() const {
    if (!state_)
      throw std::future_error(std::future_errc::no_state);
    return state_->continuations().wait();
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

  std::add_lvalue_reference_t<std::add_const_t<T>> get() {
    if (!state_)
      throw std::future_error(std::future_errc::no_state);
    wait();
    return state_->value_ref();
  }

  bool is_ready() const {
    if (!state_)
      throw std::future_error(std::future_errc::no_state);
    return state_->continuations().is_consumed();
  }

  template<typename F>
  future<detail::continuation_result_t<portable_concurrency::cxx14_v1::shared_future, F, T>>
  then(F&& f) {
    return detail::make_then_state<portable_concurrency::cxx14_v1::shared_future, T, F>(
      state_, std::forward<F>(f)
    );
  }

  // Implementation detail
  shared_future(std::shared_ptr<detail::future_state<T>>&& state) noexcept:
    state_(std::move(state))
  {}

private:
  friend detail::future_state<T>* detail::state_of<T>(shared_future<T>&);

private:
  std::shared_ptr<detail::future_state<T>> state_;
};

} // inline namespace cxx14_v1
} // namespace portable_concurrency
