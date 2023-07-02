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

template <typename R, typename... A> struct packaged_task_state {
  virtual ~packaged_task_state() = default;

  virtual void run(std::shared_ptr<shared_state<R>> &state, A &&...) = 0;
  virtual future_state<R> *get_future_state() = 0;
  virtual shared_state<R> *get_promise_state() = 0;
  virtual void abandon() = 0;
};

template <typename F, typename R, typename... A>
struct task_state final : packaged_task_state<R, A...> {
  task_state(F &&f) : func(detail::in_place_index_t<1>{}, std::forward<F>(f)) {}

  void run(std::shared_ptr<shared_state<R>> &state_ptr, A &&...a) override {
    assert(state_ptr.get() == &state);
    if (func.state() == 0)
      throw_already_satisfied();
    scope_either_cleaner<decltype(func)> claner{func};
    ::portable_concurrency::cxx14_v1::detail::set_state_value(
        state_ptr, func.get(detail::in_place_index_t<1>{}),
        std::forward<A>(a)...);
  }

  future_state<R> *get_future_state() override { return &state; }

  shared_state<R> *get_promise_state() override { return &state; }

  void abandon() override {
    func.clean();
    state.abandon();
  }

  detail::either<detail::monostate, std::decay_t<F>> func;
  shared_state<R> state;
};

} // namespace detail

template <typename R, typename... A> class packaged_task<R(A...)> {
private:
  using result_type = typename detail::add_future_t<R>::value_type;

public:
  packaged_task() noexcept = default;
  ~packaged_task() {
    if (auto state = get_state(false))
      state->abandon();
  }

  template <typename F>
  explicit packaged_task(F &&f)
      : state_{detail::in_place_index_t<1>{},
               std::make_shared<detail::task_state<F, result_type, A...>>(
                   std::forward<F>(f))} {
    static_assert(
        std::is_convertible<detail::invoke_result_t<F, A...>, R>::value,
        "F must be Callable with signature R(A...)");
  }

  packaged_task(const packaged_task &) = delete;
  packaged_task(packaged_task &&) noexcept = default;

  packaged_task &operator=(const packaged_task &) = delete;
  packaged_task &operator=(packaged_task &&rhs) noexcept {
    if (auto state = get_state(false))
      state->abandon();
    state_ = std::move(rhs.state_);
    return *this;
  }

  bool valid() const noexcept { return !state_.empty(); }

  void swap(packaged_task &other) noexcept { std::swap(state_, other.state_); }

  PC_NODISCARD detail::add_future_t<R> get_future() {
    if (state_.state() == 2u)
      detail::throw_already_retrieved();
    auto state = get_state();
    state_.emplace(detail::in_place_index_t<2>{}, state);
    return {std::shared_ptr<detail::future_state<result_type>>{
        state, state->get_future_state()}};
  }

  void operator()(A... a) {
    if (auto state = get_state()) {
      std::shared_ptr<detail::shared_state<result_type>> state_ptr{
          state, state->get_promise_state()};
      state->run(state_ptr, std::forward<A>(a)...);
    }
  }

private:
  std::shared_ptr<detail::packaged_task_state<result_type, A...>>
  get_state(bool throw_no_state = true) {
    struct {
      std::shared_ptr<detail::packaged_task_state<result_type, A...>>
      operator()(detail::monostate) {
        if (throw_no_state)
          detail::throw_no_state();
        return nullptr;
      }
      std::shared_ptr<detail::packaged_task_state<result_type, A...>>
      operator()(
          const std::shared_ptr<detail::packaged_task_state<result_type, A...>>
              &state) {
        return state;
      }
      std::shared_ptr<detail::packaged_task_state<result_type, A...>>
      operator()(
          const std::weak_ptr<detail::packaged_task_state<result_type, A...>>
              &state) {
        return state.lock();
      }

      bool throw_no_state;
    } visitor{throw_no_state};
    return state_.visit(visitor);
  }

private:
  detail::either<
      detail::monostate,
      std::shared_ptr<detail::packaged_task_state<result_type, A...>>,
      std::weak_ptr<detail::packaged_task_state<result_type, A...>>>
      state_;
};

} // namespace cxx14_v1
} // namespace portable_concurrency
