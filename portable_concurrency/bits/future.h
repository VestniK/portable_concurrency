#pragma once

#include <future>
#include <type_traits>

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
 * @brief The class template future provides a mechanism to access the result of asynchronous operations.
 */
template<typename T>
class future {
  static_assert(!detail::is_future<T>::value, "future<future<T>> and future<shared_future<T>> are not allowed");
public:
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
  future& operator= (const future&) = delete;
  /**
   * Move assignmet
   *
   * @post rhs.valid() == false
   */
  future& operator= (future&& rhs) noexcept = default;

  /**
   * Destroys associated shared state and cancel not yet started operations.
   */
  ~future() = default;

  /**
   * Creates shared_future object and move ownership on the shared state associated to this object to it
   *
   * @post this->valid() == false
   */
  shared_future<T> share() noexcept {
    return {std::move(*this)};
  }

  /**
   * @brief Get the result of asynchronyous operation stored in this future object.
   *
   * If the value is not yet set block current thread and wait for it. The value stored in the future is moved to the
   * caller.
   *
   * @post this->valid() == false
   */
  T get() {
    if (!state_)
      throw std::future_error(std::future_errc::no_state);
    wait();
    auto state = std::move(state_);
    return std::move(state->value_ref());
  }

  /**
   * Blocks curent thread until this future object becomes ready.
   */
  void wait() const {
    if (!state_)
      throw std::future_error(std::future_errc::no_state);
    state_->wait();
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

  /**
   * Checks if future has associated shared state
   *
   * @note All operations except this method, move assignment and destructor on an invalid future are UB.
   */
  bool valid() const noexcept {return static_cast<bool>(state_);}

  /**
   * Checks if associated shared state is ready
   */
  bool is_ready() const {
    if (!state_)
      throw std::future_error(std::future_errc::no_state);
    return state_->continuations_executed();
  }

  template<typename F>
  detail::cnt_future_t<F, future<T>>
  then(F&& f) {
    if (!state_)
      throw std::future_error(std::future_errc::no_state);
    return detail::make_then_state<detail::cnt_tag::then, T, F>(
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
    return detail::make_then_state<detail::cnt_tag::then, T, E, F>(
      std::move(state_), std::forward<E>(exec), std::forward<F>(f)
    );
  }

  template<typename F>
  detail::cnt_future_t<F, T> next(F&& f) {
    if (!state_)
      throw std::future_error(std::future_errc::no_state);
    return detail::make_then_state<detail::cnt_tag::next, T, F>(std::move(state_), std::forward<F>(f));
  }

  template<typename E, typename F>
  auto next(E&& exec, F&& f) -> std::enable_if_t<
    is_executor<std::decay_t<E>>::value,
    detail::cnt_future_t<F, T>
  > {
    if (!state_)
      throw std::future_error(std::future_errc::no_state);
    return detail::make_then_state<detail::cnt_tag::next, T, E, F>(
      std::move(state_), std::forward<E>(exec), std::forward<F>(f)
    );
  }

  /**
   * Prevents cancellation of the operations of this future value calculation on its destruction.
   *
   * @post this->valid() == false
   */
  void detach() {
    if (!state_)
      throw std::future_error(std::future_errc::no_state);
    auto state_ref = state_;
    state_ref->push_continuation([captured_state = std::move(state_)]() { });
  }

  // implementation detail
  future(std::shared_ptr<detail::future_state<T>>&& state) noexcept:
    state_(std::move(state))
  {}

#if defined(__cpp_coroutines)
  // Corouttines TS support
  using promise_type = promise<T>;
  bool await_ready() const noexcept {return is_ready();}
  T await_resume() {return get();}
  void await_suspend(std::experimental::coroutine_handle<> handle) {
    state_->push_continuation(std::move(handle));
  }
#endif

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
