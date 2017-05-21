#pragma once

#include <future>
#include <type_traits>

#include "fwd.h"

#include "continuation.h"
#include "shared_state.h"
#include "utils.h"
#include "wait_continuation.h"

namespace experimental {
inline namespace concurrency_v1 {

namespace detail {

template<typename T>
future_state<T>* state_of(future<T>&);

} // namespace detail

template<typename T>
class future {
public:
  future() noexcept = default;
  future(future&&) noexcept = default;
  future(const future&) = delete;

  // TODO: specification requires noexcept unwrap constructor
  future(future<future<T>>&& wrapped) {
    auto state = std::make_shared<detail::shared_state<T>>();
    wrapped.then([state](future<future<T>>&& ready_wrapped) -> void {
      try {
        auto f = ready_wrapped.get();
        if (!f.valid()) {
          return state->set_exception(std::make_exception_ptr(
            std::future_error(std::future_errc::broken_promise)
          ));
        }
        f.then([state](future<T>&& ready_f) -> void {
          try {
            state->emplace(ready_f.get());
          } catch(...) {
            state->set_exception(std::current_exception());
          }
        });
      } catch(...) {
        state->set_exception(std::current_exception());
      }
    });
    state_ = std::move(state);
  }

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
    if (state_->is_ready())
      return;

    auto waiter = std::make_shared<detail::wait_continuaton>();
    state_->set_continuation(waiter);
    waiter->wait();
  }

  template<typename Rep, typename Period>
  std::future_status wait_for(const std::chrono::duration<Rep, Period>& rel_time) const {
    if (!state_)
      throw std::future_error(std::future_errc::no_state);
    if (state_->is_ready())
      return std::future_status::ready;

    auto waiter = std::make_shared<detail::wait_continuaton>();
    state_->set_continuation(waiter);
    return waiter->wait_for(rel_time) ? std::future_status::ready : std::future_status::timeout;
  }

  template <typename Clock, typename Duration>
  std::future_status wait_until(const std::chrono::time_point<Clock, Duration>& abs_time) const {
    if (!state_)
      throw std::future_error(std::future_errc::no_state);
    if (state_->is_ready())
      return std::future_status::ready;

    auto waiter = std::make_shared<detail::wait_continuaton>();
    state_->set_continuation(waiter);
    return waiter->wait_until(abs_time) ? std::future_status::ready : std::future_status::timeout;
  }

  bool valid() const noexcept {return static_cast<bool>(state_);}

  bool is_ready() const {
    if (!state_)
      throw std::future_error(std::future_errc::no_state);
    return state_->is_ready();
  }

  template<typename F>
  auto then(F&& f) {
    if (!state_)
      throw std::future_error(std::future_errc::no_state);
    return future<detail::continuation_result_t<experimental::concurrency_v1::future, F, T>>{
      detail::continuation_state<experimental::concurrency_v1::future, F, T>::make(std::forward<F>(f), std::move(state_))
    };
  }

  // implementation detail
  future(std::shared_ptr<detail::future_state<T>>&& state) noexcept:
    state_(std::move(state))
  {}

private:
  friend class shared_future<T>;
  friend detail::future_state<T>* detail::state_of<T>(future<T>&);

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

template<>
inline
future<void>::future(future<future<void>>&& wrapped) {
  auto state = std::make_shared<detail::shared_state<void>>();
  wrapped.then([state](future<future<void>>&& ready_wrapped) -> void {
    try {
      auto f = ready_wrapped.get();
      if (!f.valid()) {
        return state->set_exception(std::make_exception_ptr(
          std::future_error(std::future_errc::broken_promise)
        ));
      }
      f.then([state](future<void>&& ready_f) -> void {
        try {
          ready_f.get();
          state->emplace();
        } catch(...) {
          state->set_exception(std::current_exception());
        }
      });
    } catch(...) {
      state->set_exception(std::current_exception());
    }
  });
  state_ = std::move(state);
}

} // inline namespace concurrency_v1
} // namespace experimental
