#pragma once

#include "fwd.h"

#include "concurrency_type_traits.h"
#include "execution.h"
#include "shared_state.h"
#include "utils.h"

namespace portable_concurrency {
inline namespace cxx14_v1 {
namespace detail {

struct inplace_executor {};
template<typename F>
void post(inplace_executor, F&& f) {f();}

} // namespace detail
} // inline namespace cxx14_v1

template<>
struct is_executor<detail::inplace_executor>: std::true_type {};

inline namespace cxx14_v1 {
namespace detail {

// small helpers

template<typename T>
std::weak_ptr<T> weak(std::shared_ptr<T> ptr) {return {std::move(ptr)};}

// Decorate different functors in order to be callable as
// `decorated_func(shared_ptr<shared_state<R>>, shared_ptr<future_state<T>>);`

namespace this_ns = ::portable_concurrency::cxx14_v1::detail;

template<typename T>
using cref_t = std::add_lvalue_reference_t<std::add_const_t<T>>;

template<typename F, typename T>
using DirectContinuation = std::enable_if_t<!is_future<cnt_result_t<F, T>>::value, F>;

template<typename F, typename T>
using UnwrappableContinuation = std::enable_if_t<is_future<cnt_result_t<F, T>>::value, F>;

template<typename R, typename T, typename F>
auto decorate_unique_then(DirectContinuation<F, future<T>>&& f, std::shared_ptr<future_state<T>>&& parent) {
  return [f = std::forward<F>(f), parent = std::move(parent)](std::shared_ptr<shared_state<R>>&& state) mutable {
    set_state_value(*state, std::move(f), future<T>{std::move(parent)});
  };
}

template<typename R, typename T, typename F>
auto decorate_unique_then(UnwrappableContinuation<F, future<T>>&& f, std::shared_ptr<future_state<T>>&& parent) {
  return [f = std::forward<F>(f), parent = std::move(parent)](std::shared_ptr<shared_state<R>>&& state) mutable {
    try {
      shared_state<R>::unwrap(state, state_of(this_ns::invoke(std::move(f), future<T>{std::move(parent)})));
    } catch (...) {
      state->set_exception(std::current_exception());
    }
  };
}

template<typename R, typename T, typename F>
auto decorate_shared_then(DirectContinuation<F, shared_future<T>>&& f, const std::shared_ptr<future_state<T>>& parent) {
  return [f = std::forward<F>(f), parent = std::shared_ptr<future_state<T>>{parent}](
    std::shared_ptr<shared_state<R>>&& state
  ) mutable {
    set_state_value(*state, std::move(f), shared_future<T>{std::move(parent)});
  };
}

template<typename R, typename T, typename F>
auto decorate_shared_then(
  UnwrappableContinuation<F, shared_future<T>>&& f,
  const std::shared_ptr<future_state<T>>& parent
) {
  return [f = std::forward<F>(f), parent = std::shared_ptr<future_state<T>>{parent}](
    std::shared_ptr<shared_state<R>>&& state
  ) mutable {
    try {
      shared_state<R>::unwrap(state, state_of(this_ns::invoke(std::move(f), shared_future<T>{std::move(parent)})));
    } catch(...) {
      state->set_exception(std::current_exception());
    }
  };
}

template<typename R, typename T, typename F>
auto decorate_unique_next(DirectContinuation<F, T>&& f, std::shared_ptr<future_state<T>>&& parent) {
  return [f = std::forward<F>(f), parent = std::move(parent)](std::shared_ptr<shared_state<R>>&& state) mutable {
    if (auto error = parent->exception()) {
      state->set_exception(std::move(error));
      return;
    }
    set_state_value(*state, std::move(f), std::move(parent->value_ref()));
  };
}

template<typename R, typename T, typename F>
auto decorate_unique_next(UnwrappableContinuation<F, T>&& f, std::shared_ptr<future_state<T>>&& parent) {
  return [f = std::forward<F>(f), parent = std::move(parent)](std::shared_ptr<shared_state<R>>&& state) mutable {
    if (auto error = parent->exception()) {
      state->set_exception(std::move(error));
      return;
    }
    try {
      shared_state<R>::unwrap(state, state_of(this_ns::invoke(std::move(f), std::move(parent->value_ref()))));
    } catch(...) {
      state->set_exception(std::current_exception());
    }
  };
}

template<typename R, typename T, typename F>
auto decorate_shared_next(DirectContinuation<F, cref_t<T>>&& f, const std::shared_ptr<future_state<T>>& parent) {
  return [f = std::forward<F>(f), parent](std::shared_ptr<shared_state<R>>&& state) mutable {
    if (auto error = parent->exception()) {
      state->set_exception(std::move(error));
      return;
    }
    set_state_value(*state, std::move(f), static_cast<cref_t<T>>(parent->value_ref()));
  };
}

template<typename R, typename T, typename F>
auto decorate_shared_next(UnwrappableContinuation<F, cref_t<T>>&& f, const std::shared_ptr<future_state<T>>& parent) {
  return [f = std::forward<F>(f), parent](std::shared_ptr<shared_state<R>>&& state) mutable {
    if (auto error = parent->exception()) {
      state->set_exception(std::move(error));
      return;
    }
    try {
      shared_state<R>::unwrap(
        state, state_of(this_ns::invoke(std::move(f), static_cast<cref_t<T>>(parent->value_ref())))
      );
    } catch(...) {
      state->set_exception(std::current_exception());
    }
  };
}

template<typename R, typename F>
auto decorate_void_next(DirectContinuation<F, void>&& f, std::shared_ptr<future_state<void>> parent) {
  return [f = std::forward<F>(f), parent = std::move(parent)](std::shared_ptr<shared_state<R>>&& state) mutable {
    if (auto error = parent->exception()) {
      state->set_exception(std::move(error));
      return;
    }
    set_state_value(*state, std::move(f));
  };
}

template<typename R, typename F>
auto decorate_void_next(UnwrappableContinuation<F, void>&& f, std::shared_ptr<future_state<void>> parent) {
  return [f = std::forward<F>(f), parent = std::move(parent)](std::shared_ptr<shared_state<R>>&& state) mutable {
    if (auto error = parent->exception()) {
      state->set_exception(std::move(error));
      return;
    }
    try {
      shared_state<R>::unwrap(state, state_of(this_ns::invoke(std::move(f))));
    } catch(...) {
      state->set_exception(std::current_exception());
    }
  };
}

// continuation state

template<typename CntState>
struct cnt_action {
  std::weak_ptr<CntState> wdata;

  cnt_action(const cnt_action&) = delete;

#if defined(__GNUC__) && __GNUC__ < 5 && !defined(__clang__)
  cnt_action(std::weak_ptr<CntState> wdata): wdata(std::move(wdata)) {}
  cnt_action(cnt_action&& rhs) noexcept: wdata(std::exchange(rhs.wdata, {})) {}
  cnt_action& operator=(cnt_action&& rhs) noexcept {
    wdata = std::exchange(rhs.wdata, {});
    return *this;
  }
#else
  cnt_action(cnt_action&&) noexcept = default;
  cnt_action& operator=(cnt_action&&) noexcept = default;
#endif

  void operator() () {
    if (auto data = std::exchange(wdata, {}).lock())
      CntState::run(std::move(data));
  }

  ~cnt_action() {
    if (auto data = wdata.lock())
      data->abandon();
  }
};

template<typename CntState, typename E>
class state_with_executor {
public:
  template<typename... A>
  state_with_executor(E&& exec, A&&... a):
    state_(std::forward<A>(a)...), exec_(in_place_index_t<1>{}, std::move(exec))
  {}

  template<typename... A>
  state_with_executor(const E& exec, A&&... a):
    state_(std::forward<A>(a)...), exec_(in_place_index_t<1>{}, exec)
  {}

  CntState& get_state() {return state_;}

  static void run(std::shared_ptr<state_with_executor> self) {
    E exec = std::move(self->exec_.get(in_place_index_t<1>{}));
    self->exec_.clean();
    CntState* state = &self->get_state();
    std::weak_ptr<CntState> weak_state = std::shared_ptr<CntState>{std::exchange(self, nullptr), state};
    post(exec, cnt_action<CntState>{std::move(weak_state)});
  }

private:
  CntState state_;
  either<detail::monostate, E> exec_;
};

template<typename R, typename F>
class cnt_state final {
public:
  cnt_state(F&& func):
    action_(in_place_index_t<1>{}, std::move(func))
  {}

  future_state<R>& get_future_state() {return state_;}

  void abandon() {
    state_.abandon();
    action_.clean();
  }

  static void run(std::shared_ptr<cnt_state> self_sp) noexcept {
    F func = std::move(self_sp->action_.get(in_place_index_t<1>{}));
    self_sp->action_.clean();
    shared_state<R>* state = &self_sp->state_;
    std::shared_ptr<shared_state<R>> state_sp{std::exchange(self_sp, nullptr), state};
    func(std::move(state_sp));
  }

private:
  shared_state<R> state_;
  either<detail::monostate, F> action_;
};

template<typename R, typename E, typename F>
auto make_then_state(continuations_stack& subscriptions, E&& exec, F&& f) {
  using cnt_data_t = state_with_executor<cnt_state<R, std::decay_t<F>>, std::decay_t<E>>;

  auto data = std::make_shared<cnt_data_t>(std::forward<E>(exec), std::forward<F>(f));
  subscriptions.push([wdata = weak(data)] {
    if (auto data = wdata.lock())
      cnt_data_t::run(std::move(data));
  });
  return std::shared_ptr<future_state<R>>{data, &data->get_state().get_future_state()};
}

} // namespace detail
} // inline namespace cxx14_v1
} // namespace portable_concurrency
