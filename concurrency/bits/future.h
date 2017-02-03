#pragma once

#include <future>
#include <type_traits>

#include "fwd.h"

#include "invoke.h"
#include "shared_state.h"
#include "utils.h"

namespace experimental {
inline namespace concurrency_v1 {

namespace detail {

template<typename T>
future<T> make_future(std::shared_ptr<shared_state<T>>&& state);

template<typename F, typename T>
using continuation_result_t = std::result_of_t<F(future<T>)>;

template<typename F, typename T>
class continuation_state:
  public shared_state<continuation_result_t<F, T>>,
  public continuation
{
public:
  using result_t = continuation_result_t<F, T>;

  continuation_state(F&& f, std::shared_ptr<shared_state<T>>&& parent):
    func_(std::forward<F>(f)),
    parent_(std::move(parent))
  {
    // using this-> to prevent ADL
    this->set_deferred_action(parent_->get_deferred_action());
  }

  static std::shared_ptr<shared_state<continuation_result_t<F, T>>> make(
    F&& func,
    std::shared_ptr<shared_state<T>>&& parent
  ) {
    auto res = std::make_shared<continuation_state>(std::forward<F>(func), std::move(parent));
    res->parent_->add_continuation(res);
    return res;
  }

  void invoke() noexcept override {
    ::experimental::concurrency_v1::detail::set_state_value(
      *this,
      std::move(func_),
      make_future(std::move(parent_))
    );
  }

private:
  std::decay_t<F> func_;
  std::shared_ptr<shared_state<T>> parent_;
};

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
    return detail::make_future(detail::continuation_state<F, T>::make(
      std::forward<F>(f), std::move(state_)
    ));
  }

private:
  friend class shared_future<T>;
  friend future<T> detail::make_future<T>(std::shared_ptr<detail::shared_state<T>>&& state);

  explicit future(std::shared_ptr<detail::shared_state<T>>&& state) noexcept:
    state_(std::move(state))
  {}

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
