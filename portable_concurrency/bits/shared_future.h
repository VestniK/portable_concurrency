#pragma once

#include <future>

#include "fwd.h"

#include "concurrency_type_traits.h"
#include "utils.h"
#include "wait_continuation.h"

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
    if (state_->is_ready())
      return;
    return state_->get_waiter().wait();
  }

  template<typename Rep, typename Period>
  std::future_status wait_for(const std::chrono::duration<Rep, Period>& rel_time) const {
    if (!state_)
      throw std::future_error(std::future_errc::no_state);
    if (state_->is_ready())
      return std::future_status::ready;
    return state_->get_waiter().wait_for(std::chrono::duration_cast<std::chrono::nanoseconds>(rel_time)) ?
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
    return state_->is_ready();
  }

  template<typename F>
  future<detail::continuation_result_t<portable_concurrency::cxx14_v1::shared_future, F, T>>
  then(F&& f) {
    using R = detail::continuation_result_t<portable_concurrency::cxx14_v1::shared_future, F, T>;
    if (!state_)
      throw std::future_error(std::future_errc::no_state);
    struct cnt_data_t {
      std::decay_t<F> func;
      std::shared_ptr<detail::future_state<T>> parent;
      detail::shared_state<R> state;

      cnt_data_t(F&& func, const std::shared_ptr<detail::future_state<T>>& parent):
        func(std::forward<F>(func)), parent(parent)
      {}
    };
    auto cnt_data = std::make_shared<cnt_data_t>(
      std::forward<F>(f), state_
    );
    cnt_data->parent->add_continuation([cnt_data]{
      detail::set_state_value(
        std::shared_ptr<detail::shared_state<R>>(cnt_data, &cnt_data->state),
        std::move(cnt_data->func), shared_future{std::move(cnt_data->parent)}
      );
    });
    return {std::shared_ptr<detail::future_state<R>>(cnt_data, &cnt_data->state)};
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
