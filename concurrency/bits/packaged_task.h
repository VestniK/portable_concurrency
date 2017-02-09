#pragma once

#include <memory>

#include "fwd.h"

#include "future.h"
#include "shared_state.h"
#include "utils.h"

namespace experimental {
inline namespace concurrency_v1 {

namespace detail {

template<typename... A>
class invokable {
public:
  virtual ~invokable() = default;
  virtual void invoke(A...) = 0;
};

template<typename R, typename... A>
class task_state_base:
  public shared_state<R>,
  public invokable<A...>
{
public:
  virtual std::shared_ptr<task_state_base> reset() = 0;
};

template<typename F, typename R, typename... A>
class task_state: public task_state_base<R, A...> {
public:
  template<typename U>
  task_state(U&& f): func_(std::forward<U>(f)) {}

  void invoke(A... a) override {
    ::experimental::concurrency_v1::detail::set_state_value(*this, func_, std::forward<A>(a)...);
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

using ignore_t = decltype(std::ignore);

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
    return detail::make_future(std::static_pointer_cast<detail::shared_state<R>>(state_));
  }

  void operator() (A... a) {
    if (!state_)
      throw std::future_error(std::future_errc::no_state);
    state_->invoke(a...);
  }

  void reset() {
    state_ = state_->reset();
  }

private:
  friend class packaged_task<ignore_t(A...)>;

  std::shared_ptr<detail::task_state_base<R, A...>> state_;
};

template<typename... A>
class packaged_task<ignore_t(A...)> {
public:
  packaged_task() = default;

  template<typename R>
  packaged_task(packaged_task<R(A...)>&& task): state_(std::move(task.state_)) {
    if (state_ && state_.use_count() == 1)
      throw std::future_error(std::future_errc::broken_promise);
  }

  packaged_task(const packaged_task&) = delete;
  packaged_task(packaged_task&&) noexcept = default;

  packaged_task& operator= (const packaged_task&) = delete;
  packaged_task& operator= (packaged_task&&) noexcept = default;

  bool valid() const noexcept {return static_cast<bool>(state_);}

  void swap(packaged_task& other) noexcept {state_.swap(other.state_);}

  void operator() (A... a) {
    if (!state_)
      throw std::future_error(std::future_errc::no_state);
    state_->invoke(a...);
  }
private:
  std::shared_ptr<detail::invokable<A...>> state_;
};

} // inline namespace concurrency_v1
} // namespace experimental

