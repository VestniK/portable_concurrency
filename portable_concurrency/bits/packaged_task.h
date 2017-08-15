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
class task_state_base: public shared_state<R> {
public:
  virtual void invoke(const std::shared_ptr<shared_state<R>>& self, A...) = 0;
  virtual std::shared_ptr<task_state_base> reset() = 0;
};

template<typename F, typename R, typename... A>
class task_state: public task_state_base<R, A...> {
public:
  template<typename U>
  task_state(U&& f): func_(std::forward<U>(f)) {}

  void invoke(const std::shared_ptr<shared_state<R>>& self, A... a) override {
    ::portable_concurrency::cxx14_v1::detail::set_state_value(self, func_, std::forward<A>(a)...);
  }

  std::shared_ptr<task_state_base<R, A...>> reset() override {
    return std::shared_ptr<task_state_base<R, A...>>{
      new task_state{std::move(func_)}
    };
  }

private:
  std::decay_t<F> func_;
};

} // namespace detail

template<typename R, typename... A>
class packaged_task<R(A...)> {
public:
  packaged_task() noexcept = default;

  template<typename F>
  explicit packaged_task(F&& f):
    state_(new detail::task_state<F, R, A...>{std::forward<F>(f)})
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
    if (state_.use_count() != 1)
      throw std::future_error(std::future_errc::future_already_retrieved);
    return {std::static_pointer_cast<detail::shared_state<R>>(state_)};
  }

  void operator() (A... a) {
    if (!state_)
      throw std::future_error(std::future_errc::no_state);
    state_->invoke(state_, a...);
  }

  void reset() {
    state_ = state_->reset();
  }

private:
  std::shared_ptr<detail::task_state_base<R, A...>> state_;
};

} // inline namespace cxx14_v1
} // namespace portable_concurrency

