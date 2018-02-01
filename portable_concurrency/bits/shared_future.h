#pragma once

#include <future>

#if defined(__cpp_coroutines)
#include <experimental/coroutine>
#endif

#include "fwd.h"

#include "concurrency_type_traits.h"
#include "execution.h"
#include "future_state.h"
#include "then.h"

namespace portable_concurrency {
inline namespace cxx14_v1 {

/**
 * @ingroup future_hdr
 * @brief The class template shared_future provides a mechanism to access the result of asynchronous operations.
 */
template<typename T>
class shared_future {
  static_assert (
    !detail::is_future<T>::value,
    "shared_future<future<T> and shared_future<shared_future<T>> are not allowed"
  );
  using get_result_type = std::add_lvalue_reference_t<std::add_const_t<T>>;
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
    return state_->wait();
  }

  template<typename Rep, typename Period>
  std::future_status wait_for(const std::chrono::duration<Rep, Period>& rel_time) const {
    if (!state_)
      throw std::future_error(std::future_errc::no_state);
    return state_->wait_for(std::chrono::duration_cast<std::chrono::nanoseconds>(rel_time)) ?
      std::future_status::ready:
      std::future_status::timeout
    ;
  }

  template <typename Clock, typename Duration>
  std::future_status wait_until(const std::chrono::time_point<Clock, Duration>& abs_time) const {
    return wait_for(abs_time - Clock::now());
  }

  bool valid() const noexcept {return static_cast<bool>(state_);}

  get_result_type get() const {
    if (!state_)
      throw std::future_error(std::future_errc::no_state);
    wait();
    return state_->value_ref();
  }

  bool is_ready() const {
    if (!state_)
      throw std::future_error(std::future_errc::no_state);
    return state_->continuations_executed();
  }

  template<typename F>
  detail::cnt_future_t<F, shared_future<T>>
  then(F&& f) {
    if (!state_)
      throw std::future_error(std::future_errc::no_state);
    return detail::make_then_state<detail::cnt_tag::shared_then, T, F>(
      state_, std::forward<F>(f)
    );
  }

  template<typename E, typename F>
  auto then(E&& exec, F&& f) -> std::enable_if_t<
    is_executor<std::decay_t<E>>::value,
    detail::cnt_future_t<F, shared_future<T>>
  > {
    if (!state_)
      throw std::future_error(std::future_errc::no_state);
    return detail::make_then_state<detail::cnt_tag::shared_then, T, E, F>(
      std::move(state_), std::forward<E>(exec), std::forward<F>(f)
    );
  }

  template<typename F>
  detail::cnt_future_t<F, get_result_type> next(F&& f) {
    if (!state_)
      throw std::future_error(std::future_errc::no_state);
    return detail::make_then_state<detail::cnt_tag::shared_next, T, F>(state_, std::forward<F>(f));
  }

  template<typename E, typename F>
  auto next(E&& exec, F&& f) -> std::enable_if_t<
    is_executor<std::decay_t<E>>::value,
    detail::cnt_future_t<F, get_result_type>
  > {
    if (!state_)
      throw std::future_error(std::future_errc::no_state);
    return detail::make_then_state<detail::cnt_tag::shared_next, T, E, F>(
      state_, std::forward<E>(exec), std::forward<F>(f)
    );
  }

  /**
   * Prevents cancelation of the operations of this shared_future value calculation on its destruction.
   *
   * @post this->valid() == false
   */
  void detach() {
    if (!state_)
      throw std::future_error(std::future_errc::no_state);
    auto stateref = state_;
    stateref->push_continuation([captured_state = std::move(state_)]() {});
  }

  // Implementation detail
  shared_future(std::shared_ptr<detail::future_state<T>>&& state) noexcept:
    state_(std::move(state))
  {}

#if defined(__cpp_coroutines)
  // Corouttines TS support
  using promise_type = promise<T>;
  bool await_ready() const noexcept {return is_ready();}
  decltype(auto) await_resume() {return get();}
  void await_suspend(std::experimental::coroutine_handle<> handle) {
    state_->continuations().push(std::move(handle));
  }
#endif

private:
  friend std::shared_ptr<detail::future_state<T>>& detail::state_of<T>(shared_future<T>&);

private:
  std::shared_ptr<detail::future_state<T>> state_;
};

template<>
inline
std::add_lvalue_reference_t<std::add_const_t<void>> shared_future<void>::get() const {
  if (!state_)
    throw std::future_error(std::future_errc::no_state);
  wait();
  state_->value_ref();
}

namespace detail {

template<typename T>
std::shared_ptr<future_state<T>>& state_of(shared_future<T>& f) {
  return f.state_;
}

} // namespace detail

} // inline namespace cxx14_v1
} // namespace portable_concurrency
