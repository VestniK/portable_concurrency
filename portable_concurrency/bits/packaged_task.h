#pragma once

#include <memory>

#include "fwd.h"

#include "future.h"
#include "shared_state.h"
#include "utils.h"

namespace portable_concurrency {
inline namespace cxx14_v1 {

namespace detail {

template<typename R, typename... A>
struct packaged_task_state {
  virtual ~packaged_task_state() = default;

  virtual void run(A...) = 0;
  virtual future_state<R>* take_state() = 0;
  virtual void abandon() = 0;
};

template<typename F, typename R, typename... A>
struct task_state: packaged_task_state<R, A...> {
  task_state(F&& f): func(std::forward<F>(f)) {}

  void run(A... a) override {
    ::portable_concurrency::cxx14_v1::detail::set_state_value(state, func, std::forward<A>(a)...);
  }

  future_state<R>* take_state() override {
    if (std::exchange(state_taken, true))
      return nullptr;
    return &state;
  }

  void abandon() override {
    if (!state.continuations().executed())
      state.set_exception(std::make_exception_ptr(std::future_error{std::future_errc::broken_promise}));
  };

  std::decay_t<F> func;
  shared_state<R> state;
  bool state_taken = false;
};

} // namespace detail

template<typename R, typename... A>
class packaged_task<R(A...)> {
public:
  packaged_task() noexcept = default;
  ~packaged_task() {
    if (auto state = state_.lock())
      state->abandon();
  }

  template<typename F>
  explicit packaged_task(F&& f) {
    auto state = std::make_shared<detail::task_state<F, R, A...>>(std::forward<F>(f));
    future_state_ = std::shared_ptr<detail::future_state<R>>{state, state->take_state()};
    state_ = std::move(state);
  }

  packaged_task(const packaged_task&) = delete;
  packaged_task(packaged_task&&) noexcept = default;

  packaged_task& operator= (const packaged_task&) = delete;
  packaged_task& operator= (packaged_task&&) noexcept = default;

  bool valid() const noexcept {
    std::weak_ptr<detail::packaged_task_state<R, A...>> empty;
    return empty.owner_before(state_) || state_.owner_before(empty);
  }

  void swap(packaged_task& other) noexcept {
    std::swap(state_, other.state_);
    std::swap(future_state_, other.future_state_);
  }

  future<R> get_future() {
    if (!valid())
      throw std::future_error(std::future_errc::no_state);
    if (!future_state_)
      throw std::future_error(std::future_errc::future_already_retrieved);
    return {std::move(future_state_)};
  }

  void operator() (A... a) {
    if (auto state = get_state())
      state->run(a...);
  }

private:
  std::shared_ptr<detail::packaged_task_state<R, A...>> get_state() {
    auto state = state_.lock();
    if (!state && !valid())
      throw std::future_error{std::future_errc::no_state};
    return state;
  }

private:
  std::weak_ptr<detail::packaged_task_state<R, A...>> state_;
  std::shared_ptr<detail::future_state<R>> future_state_;
};

} // inline namespace cxx14_v1
} // namespace portable_concurrency

