#pragma once

#include <memory>

#include "fwd.h"

#include "either.h"
#include "future.h"
#include "shared_state.h"
#include "utils.h"

namespace portable_concurrency {
inline namespace cxx14_v1 {

namespace detail {

template<typename R, typename... A>
struct packaged_task_state {
  virtual ~packaged_task_state() = default;

  virtual void run(A&&...) = 0;
  virtual future_state<R>* get_future_state() = 0;
  virtual void abandon() = 0;
};

template<typename F, typename R, typename... A>
struct task_state final: packaged_task_state<R, A...> {
  task_state(F&& f): func(std::forward<F>(f)) {}

  void run(A&&... a) override {
    ::portable_concurrency::cxx14_v1::detail::set_state_value(state, func, std::forward<A>(a)...);
  }

  future_state<R>* get_future_state() override {
    return &state;
  }

  void abandon() override {
    if (!state.continuations().executed())
      state.set_exception(std::make_exception_ptr(std::future_error{std::future_errc::broken_promise}));
  };

  std::decay_t<F> func;
  shared_state<R> state;
};

} // namespace detail

template<typename R, typename... A>
class packaged_task<R(A...)> {
public:
  packaged_task() noexcept = default;
  ~packaged_task() {
    if (auto state = get_state(false))
      state->abandon();
  }

  template<typename F>
  explicit packaged_task(F&& f)
    : state_{detail::in_place_index_t<1>{}, std::make_shared<detail::task_state<F, R, A...>>(std::forward<F>(f))}
  {}

  packaged_task(const packaged_task&) = delete;
  packaged_task(packaged_task&&) noexcept = default;

  packaged_task& operator= (const packaged_task&) = delete;
  packaged_task& operator= (packaged_task&&) noexcept = default;

  bool valid() const noexcept {
    return !state_.empty();
  }

  void swap(packaged_task& other) noexcept {
    auto tmp = std::move(state_);
    state_ = std::move(other.state_);
    other.state_ = std::move(tmp);
  }

  future<R> get_future() {
    if (state_.state() == 2u)
      throw std::future_error(std::future_errc::future_already_retrieved);
    auto state = get_state();
    state_.emplace(detail::in_place_index_t<2>{}, state);
    return {std::shared_ptr<detail::future_state<R>>{state, state->get_future_state()}};
  }

  void operator() (A... a) {
    if (auto state = get_state())
      state->run(std::forward<A>(a)...);
  }

private:
  std::shared_ptr<detail::packaged_task_state<R, A...>> get_state(bool throw_no_state = true) {
    struct {
      std::shared_ptr<detail::packaged_task_state<R, A...>> operator() (detail::monostate) {
        if (throw_no_state)
          throw std::future_error(std::future_errc::no_state);
        return nullptr;
      }
      std::shared_ptr<detail::packaged_task_state<R, A...>> operator() (
        const std::shared_ptr<detail::packaged_task_state<R, A...>>& state
      ) {
        return state;
      }
      std::shared_ptr<detail::packaged_task_state<R, A...>> operator() (
        const std::weak_ptr<detail::packaged_task_state<R, A...>>& state
      ) {
        return state.lock();
      }

      bool throw_no_state;
    } visitor{throw_no_state};
    return state_.visit(visitor);
  }

private:
  detail::either<
    detail::monostate,
    std::shared_ptr<detail::packaged_task_state<R, A...>>,
    std::weak_ptr<detail::packaged_task_state<R, A...>>
  > state_;
};

} // inline namespace cxx14_v1
} // namespace portable_concurrency

