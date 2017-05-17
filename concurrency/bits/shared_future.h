#pragma once

#include <future>

#include "fwd.h"

#include "continuation.h"
#include "utils.h"
#include "wait_continuation.h"

namespace experimental {
inline namespace concurrency_v1 {

namespace detail {

template<typename T>
shared_state<T>* state_of(shared_future<T>&);

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

    auto waiter = std::make_shared<detail::wait_continuaton>();
    // TODO: prevent multiple waiters on the same state for shared_future
    state_->add_continuation(waiter);
    waiter->wait();
  }

  template<typename Rep, typename Period>
  std::future_status wait_for(const std::chrono::duration<Rep, Period>& rel_time) const {
    if (!state_)
      throw std::future_error(std::future_errc::no_state);
    if (state_->is_ready())
      return std::future_status::ready;

    auto waiter = std::make_shared<detail::wait_continuaton>();
    // TODO: prevent multiple waiters on the same state for shared_future
    state_->add_continuation(waiter);
    return waiter->wait_for(rel_time) ? std::future_status::ready : std::future_status::timeout;
  }

  template <typename Clock, typename Duration>
  std::future_status wait_until(const std::chrono::time_point<Clock, Duration>& abs_time) const {
    if (!state_)
      throw std::future_error(std::future_errc::no_state);
    if (state_->is_ready())
      return std::future_status::ready;

    auto waiter = std::make_shared<detail::wait_continuaton>();
    // TODO: prevent multiple waiters on the same state for shared_future
    state_->add_continuation(waiter);
    return waiter->wait_until(abs_time) ? std::future_status::ready : std::future_status::timeout;
  }

  bool valid() const noexcept {return static_cast<bool>(state_);}

  std::add_lvalue_reference_t<std::add_const_t<T>> get() {
    if (!state_)
      throw std::future_error(std::future_errc::no_state);
    return state_->get();
  }

  bool is_ready() const {
    if (!state_)
      throw std::future_error(std::future_errc::no_state);
    return state_->is_ready();
  }

  template<typename F>
  auto then(F&& f) {
    if (!state_)
      throw std::future_error(std::future_errc::no_state);
    return future<detail::continuation_result_t<experimental::concurrency_v1::shared_future, F, T>>{
      detail::continuation_state<experimental::concurrency_v1::shared_future, F, T>::make(
        std::forward<F>(f), detail::decay_copy(state_)
      )
    };
  }

  // Implementation detail
  shared_future(std::shared_ptr<detail::shared_state<T>>&& state) noexcept:
    state_(std::move(state))
  {}

  // Implementation detail
  shared_future(const std::shared_ptr<detail::shared_state<T>>& state) noexcept:
    state_(state)
  {}

private:
  friend detail::shared_state<T>* detail::state_of<T>(shared_future<T>&);

private:
  std::shared_ptr<detail::shared_state<T>> state_;
};

} // inline namespace concurrency_v1
} // namespace experimental
