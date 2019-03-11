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
 * @brief The class template future provides a mechanism to access the result of asynchronous operations.
 */
template <typename T>
class future {
  static_assert(!detail::is_future<T>::value, "future<future<T>> and future<shared_future<T>> are not allowed");

public:
  using value_type = T;

  /**
   * Constructs invalid future object
   *
   * @post this->valid() == false
   */
  future() noexcept = default;
  /**
   * Move constructor
   *
   * @post rhs.valid() == false
   */
  future(future&& rhs) noexcept = default;
  /**
   * Copy constructor is explicitly deleted. Future object is not copyable.
   */
  future(const future&) = delete;

  /**
   * Copy assignment operator is explicitly deleted. Future object is not copyable.
   */
  future& operator=(const future&) = delete;
  /**
   * Move assignmet
   *
   * @post rhs.valid() == false
   */
  future& operator=(future&& rhs) noexcept = default;

  /**
   * Destroys associated shared state and cancel not yet started operations.
   */
  ~future() = default;

  /**
   * Creates shared_future object and move ownership on the shared state associated to this object to it
   *
   * @post this->valid() == false
   */
  shared_future<T> share() noexcept;

  /**
   * @brief Get the result of asynchronyous operation stored in this future object.
   *
   * If the value is not yet set block current thread and wait for it. The value stored in the future is moved to the
   * caller.
   *
   * @post this->valid() == false
   */
  T get();

  /**
   * Blocks curent thread until this future object becomes ready.
   */
  void wait() const;

  template <typename Rep, typename Period>
  future_status wait_for(const std::chrono::duration<Rep, Period>& rel_time) const;

  template <typename Clock, typename Duration>
  future_status wait_until(const std::chrono::time_point<Clock, Duration>& abs_time) const;

  /**
   * Checks if future has associated shared state
   *
   * @note All operations except this method, move assignment and destructor on an invalid future are UB.
   */
  bool valid() const noexcept;

  /**
   * Checks if associated shared state is ready
   */
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
  PC_NODISCARD detail::cnt_future_t<F, future<T>> then(F&& f);

  template <typename F>
  PC_NODISCARD detail::add_future_t<detail::promise_arg_t<F, future>> then(F&& f);

  template <typename E, typename F>
  PC_NODISCARD detail::cnt_future_t<F, future<T>> then(E&& exec, F&& f);

  template <typename E, typename F>
  PC_NODISCARD detail::add_future_t<detail::promise_arg_t<F, future>> then(E&& exec, F&& f);

  template <typename F>
  PC_NODISCARD detail::cnt_future_t<F, T> next(F&& f);

  template <typename E, typename F>
  PC_NODISCARD detail::cnt_future_t<F, T> next(E&& exec, F&& f);

  /**
   * Prevents cancellation of the operations of this future value calculation on its destruction.
   *
   * @post this->valid() == false
   */
  future detach();

  // implementation detail
  future(std::shared_ptr<detail::future_state<T>>&& state) noexcept;

#if defined(__cpp_coroutines)
  // Corouttines TS support
  using promise_type = promise<T>;
  bool await_ready() const noexcept;
  T await_resume();
  void await_suspend(std::experimental::coroutine_handle<> handle);
#endif

private:
  friend class shared_future<T>;
  friend std::shared_ptr<detail::future_state<T>>& detail::state_of<T>(future<T>&);
  friend std::shared_ptr<detail::future_state<T>> detail::state_of<T>(future<T>&&);

private:
  std::shared_ptr<detail::future_state<T>> state_;
};

template <>
void future<void>::get();

} // namespace cxx14_v1
} // namespace portable_concurrency
