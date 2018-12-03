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
  using get_result_type = std::add_lvalue_reference_t<std::add_const_t<T>>;

public:
  shared_future() noexcept = default;
  shared_future(const shared_future&) noexcept = default;
  shared_future(shared_future&&) noexcept = default;
  shared_future(future<T>&& rhs) noexcept;

  shared_future& operator=(const shared_future&) noexcept = default;
  shared_future& operator=(shared_future&&) noexcept = default;

  ~shared_future() = default;

  void wait() const;

  template <typename Rep, typename Period>
  future_status wait_for(const std::chrono::duration<Rep, Period>& rel_time) const;

  template <typename Clock, typename Duration>
  future_status wait_until(const std::chrono::time_point<Clock, Duration>& abs_time) const;

  bool valid() const noexcept;

  get_result_type get() const;

  bool is_ready() const;

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
  void detach();

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
