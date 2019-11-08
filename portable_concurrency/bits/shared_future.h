#pragma once

#include <chrono>
#include <memory>
#include <type_traits>

#if defined(__cpp_coroutines)
#include <experimental/coroutine>
#endif

#include "fwd.h"

#include "concurrency_type_traits.h"

#include <portable_concurrency/bits/config.h>

namespace portable_concurrency {
inline namespace cxx14_v1 {

/**
 * @ingroup future_hdr
 * @brief The class template shared_future provides a mechanism to access the result of asynchronous operations.
 */
template <typename T>
class shared_future {
  static_assert(
      !detail::is_future<T>::value, "shared_future<future<T> and shared_future<shared_future<T>> are not allowed");
  using get_result_type =
      std::conditional_t<std::is_void<T>::value, void, std::add_lvalue_reference_t<std::add_const_t<T>>>;

public:
  using value_type = T;

  shared_future() noexcept = default;
  shared_future(const shared_future&) noexcept = default;
  shared_future(shared_future&&) noexcept = default;
  shared_future(future<T>&& rhs) noexcept;

  shared_future& operator=(const shared_future&) noexcept = default;
  shared_future& operator=(shared_future&&) noexcept = default;

  ~shared_future() = default;

  void wait() const;

#if !defined(PC_NO_DEPRECATED)
  template <typename Rep, typename Period>
  [[deprecated("Use pc::timed_waiter instead")]] future_status wait_for(
      const std::chrono::duration<Rep, Period>& rel_time) const;

  template <typename Clock, typename Duration>
  [[deprecated("Use pc::timed_waiter instead")]] future_status wait_until(
      const std::chrono::time_point<Clock, Duration>& abs_time) const;
#endif

  bool valid() const noexcept;

  get_result_type get() const;

  bool is_ready() const;

  /**
   * Adds notification function to be called when this future object becomes ready.
   *
   * Equivalent to `this->notify(inplace_executor, notification)`.
   *
   * @note Thread of `notification` execution is unspecified.
   */
  template <typename F>
  void notify(F&& notification);

  /**
   * Adds notification function to be called when this future object becomes ready.
   *
   * Notification function must meet `MoveConstructable`, `MoveAssignable` and `Callable` (with signature `void()`)
   * standard library requirements.
   *
   * Both `exec` and `notification` objects are decay copied on the caller thread. Once this future object becomes ready
   * `post(exec, std::move(notification))` is executed on unspecified thread. Implementation provide strict guaranty
   * that `notification` is scheduled for execution at most once. If `this->is_ready() == true` then `notification` is
   * scheduled for execution immediatelly.
   */
  template <typename E, typename F>
  void notify(E&& exec, F&& notification);

  template <typename F>
  PC_NODISCARD detail::cnt_future_t<F, shared_future<T>> then(F&& f) const;

  template <typename E, typename F>
  PC_NODISCARD detail::cnt_future_t<F, shared_future<T>> then(E&& exec, F&& f) const;

  template <typename F>
  PC_NODISCARD detail::cnt_future_t<F, get_result_type> next(F&& f) const;

  template <typename E, typename F>
  PC_NODISCARD detail::cnt_future_t<F, get_result_type> next(E&& exec, F&& f) const;

  template <typename F>
  PC_NODISCARD detail::add_future_t<detail::promise_arg_t<F, shared_future<T>>> then(F&& f);

  template <typename E, typename F>
  PC_NODISCARD detail::add_future_t<detail::promise_arg_t<F, shared_future<T>>> then(E&& exec, F&& f);

  /**
   * Prevents cancelation of the operations of this shared_future value calculation on its destruction.
   *
   * @post this->valid() == false
   */
  shared_future detach();

  // Implementation detail
  shared_future(std::shared_ptr<detail::future_state<T>>&& state) noexcept;

#if defined(__cpp_coroutines)
  // Corouttines TS support
  using promise_type = promise<T>;
  bool await_ready() const noexcept;
  get_result_type await_resume() const;
  void await_suspend(std::experimental::coroutine_handle<> handle) const;
#endif

private:
  friend std::shared_ptr<detail::future_state<T>>& detail::state_of<T>(shared_future<T>&);
  friend std::shared_ptr<detail::future_state<T>> detail::state_of<T>(shared_future<T>&&);

private:
  std::shared_ptr<detail::future_state<T>> state_;
};

template <>
typename shared_future<void>::get_result_type shared_future<void>::get() const;

} // namespace cxx14_v1
} // namespace portable_concurrency
