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
void post(inplace_executor, F&& f) {portable_concurrency::detail::invoke(std::forward<F>(f));}

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

template<typename E, typename F>
struct cnt_closure {
  cnt_closure(E&& exec, F&& func):
    exec(std::move(exec)), func(std::move(func))
  {}

  E exec;
  F func;
};

template<typename CntState>
struct cnt_action {
  std::weak_ptr<CntState> wdata;

  cnt_action(const cnt_action&) = delete;

#if defined(__GNUC__) && __GNUC__ < 5
  cnt_action(std::weak_ptr<CntState> wdata): wdata(std::move(wdata)) {}
  cnt_action(cnt_action&& rhs) noexcept: wdata(std::move(rhs.wdata)) {rhs.wdata.reset();}
  cnt_action& operator=(cnt_action&& rhs) noexcept {
    wdata = std::move(rhs.wdata);
    rhs.wdata.reset();
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

template<typename R, typename E, typename F>
class cnt_state final {
public:
  cnt_state(E exec, F&& func):
    action_(in_place_index_t<1>{}, std::move(exec), std::move(func))
  {}

  future_state<R>* get_future_state() {return &state_;}

  void abandon() {
    if (state_.continuations().executed())
      return;
    action_.clean();
    state_.set_exception(std::make_exception_ptr(std::future_error{std::future_errc::broken_promise}));
  }

  static void schedule(std::shared_ptr<cnt_state> self_sp) {
    E exec = self_sp->action_.get(in_place_index_t<1>{}).exec;
    std::weak_ptr<cnt_state> weak_self = std::exchange(self_sp, nullptr);
    post(exec, cnt_action<cnt_state>{std::move(weak_self)});
  }

  static void run(std::shared_ptr<cnt_state> self_sp) noexcept {
    F func = std::move(self_sp->action_.get(in_place_index_t<1>{}).func);
    self_sp->action_.clean();
    shared_state<R>* state = &self_sp->state_;
    std::shared_ptr<shared_state<R>> state_sp{std::exchange(self_sp, nullptr), state};
    func(std::move(state_sp));
  }

private:
  shared_state<R> state_;
  either<detail::monostate, cnt_closure<E, F>> action_;
};

template<typename R, typename E, typename F>
auto make_then_state(continuations_stack& subscriptions, E&& exec, F&& f) {
  using cnt_data_t = cnt_state<R, std::decay_t<E>, std::decay_t<F>>;

  auto data = std::make_shared<cnt_data_t>(std::forward<E>(exec), std::forward<F>(f));
  subscriptions.push([wdata = weak(data)] {
    if (auto data = wdata.lock())
      cnt_data_t::schedule(std::move(data));
  });
  return std::shared_ptr<future_state<R>>{data, data->get_future_state()};
}

} // namespace detail
} // inline namespace cxx14_v1
} // namespace portable_concurrency
