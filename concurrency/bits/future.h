#pragma once

#include <future>

#include "fwd.h"

#include "invoke.h"
#include "shared_state.h"
#include "postponed_action.h"
#include "utils.h"

namespace experimental {
inline namespace concurrency_v1 {

namespace detail {

template<typename T>
future<T> make_future(std::shared_ptr<shared_state<T>>&& state);

template<typename F, typename T>
using continuation_result_t = std::result_of_t<F(future<T>)>;

template<typename F, typename T, typename R>
class future_continuation_action: public erased_action {
  static_assert(std::is_same<R, std::result_of_t<F(future<T>)>>::value, "Incorrect continuation return type");
public:
  future_continuation_action(
    F&& f,
    std::shared_ptr<shared_state<T>>&& parent,
    const std::shared_ptr<shared_state<R>>& s
  ):
    func(std::forward<F>(f)),
    parent_state(std::move(parent)),
    state(s)
  {
    if (!parent_state->is_deferred())
      return;
    std::weak_ptr<shared_state<T>> weak_parent = parent_state;
    state->set_wait_action([weak_parent]() {
      if (auto p = weak_parent.lock())
        p->wait();
    });
  }

  void invoke() override try {
    state->emplace(::experimental::concurrency_v1::detail::invoke(
      std::move(func),
      make_future(std::move(parent_state)))
    );
  } catch (...) {
    state->set_exception(std::current_exception());
  }

private:
  std::decay_t<F> func;
  std::shared_ptr<shared_state<T>> parent_state;
  std::shared_ptr<shared_state<R>> state;
};

template<typename F, typename T>
class future_continuation_action<F, T, void>: public erased_action {
  static_assert(std::is_void<std::result_of_t<F(future<T>)>>::value, "Incorrect continuation return type");
public:
  future_continuation_action(
    F&& f,
    std::shared_ptr<shared_state<T>>&& parent,
    const std::shared_ptr<shared_state<void>>& s
  ):
    func(std::forward<F>(f)),
    parent_state(std::move(parent)),
    state(s)
  {
    if (!parent_state->is_deferred())
      return;
    std::weak_ptr<shared_state<T>> weak_parent = parent_state;
    state->set_wait_action([weak_parent]() {
      if (auto p = weak_parent.lock())
        p->wait();
    });
  }

  void invoke() override try {
    ::experimental::concurrency_v1::detail::invoke(
      std::move(func),
      make_future(std::move(parent_state))
    );
    state->emplace();
  } catch (...) {
    state->set_exception(std::current_exception());
  }

private:
  std::decay_t<F> func;
  std::shared_ptr<shared_state<T>> parent_state;
  std::shared_ptr<shared_state<void>> state;
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
  future& operator= (future&& rhs) noexcept {
    state_ = std::move(rhs.state_);
    rhs.state_ = nullptr;
    return *this;
  }

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
    auto state = state_;
    using R = detail::continuation_result_t<F, T>;
    auto cnt_state = std::make_shared<detail::shared_state<R>>();
    state->add_continuation(std::unique_ptr<detail::erased_action>{
      new detail::future_continuation_action<F, T, R>{std::forward<F>(f), std::move(state_), cnt_state}
    });
    return detail::make_future<R>(std::move(cnt_state));
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
