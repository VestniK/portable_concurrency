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

  std::decay_t<F> func;
  shared_state<R> state;
  bool state_taken = false;
};

} // namespace detail

template<typename R, typename... A>
class packaged_task<R(A...)> {
public:
  packaged_task() noexcept = default;

  template<typename F>
  explicit packaged_task(F&& f):
    state_(std::make_shared<detail::task_state<F, R, A...>>(std::forward<F>(f)))
  {}

  packaged_task(const packaged_task&) = delete;
  packaged_task(packaged_task&&) noexcept = default;

  packaged_task& operator= (const packaged_task&) = delete;
  packaged_task& operator= (packaged_task&&) noexcept = default;

  bool valid() const noexcept {return static_cast<bool>(state_);}

  void swap(packaged_task& other) noexcept {state_.swap(other.state_);}

  future<R> get_future() {
    if (!state_)
      throw std::future_error(std::future_errc::no_state);
    auto* state_ptr = state_->take_state();
    if (!state_ptr)
      throw std::future_error(std::future_errc::future_already_retrieved);
    return {std::shared_ptr<detail::future_state<R>>{state_, state_ptr}};
  }

  void operator() (A... a) {
    if (!state_)
      throw std::future_error(std::future_errc::no_state);
    state_->run(a...);
  }

private:
  std::shared_ptr<detail::packaged_task_state<R, A...>> state_;
};

} // inline namespace cxx14_v1
} // namespace portable_concurrency

