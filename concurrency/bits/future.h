#pragma once

#include <future>
#include <type_traits>

#include "fwd.h"

#include "continuation.h"
#include "shared_state.h"
#include "utils.h"

namespace experimental {
inline namespace concurrency_v1 {

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
    auto state = std::move(state_);
    return state->get();
  }

  void wait() const {
    if (!state_)
      throw std::future_error(std::future_errc::no_state);
    state_->wait();
  }

  template<typename Rep, typename Period>
  std::future_status wait_for(const std::chrono::duration<Rep, Period>& rel_time) const {
    if (!state_)
      throw std::future_error(std::future_errc::no_state);
    return state_->wait_for(rel_time);
  }

  template <typename Clock, typename Duration>
  std::future_status wait_until(const std::chrono::time_point<Clock, Duration>& abs_time) const {
    if (!state_)
      throw std::future_error(std::future_errc::no_state);
    return state_->wait_until(abs_time);
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
    return future<detail::continuation_result_t<future, F, T>>{
      detail::continuation_state<future, F, T>::make(std::forward<F>(f), std::move(state_))
    };
  }

  // implementation detail
  future(std::shared_ptr<detail::shared_state<T>>&& state) noexcept:
    state_(std::move(state))
  {}

private:
  friend class shared_future<T>;

private:
  std::shared_ptr<detail::shared_state<T>> state_;
};

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
