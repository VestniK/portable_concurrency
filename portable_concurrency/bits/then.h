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
auto decorate_unique_then(DirectContinuation<F, future<T>>&& f) {
  return [f = std::forward<F>(f)](
    std::shared_ptr<shared_state<R>>&& state,
    std::shared_ptr<future_state<T>>&& parent
  ) mutable {
    set_state_value(*state, std::move(f), future<T>{std::move(parent)});
  };
}

template<typename R, typename T, typename F>
auto decorate_unique_then(UnwrappableContinuation<F, future<T>>&& f) {
  return [f = std::forward<F>(f)](
    std::shared_ptr<shared_state<R>>&& state,
    std::shared_ptr<future_state<T>>&& parent
  ) mutable {
    shared_state<R>::unwrap(state, state_of(this_ns::invoke(std::move(f), future<T>{std::move(parent)})));
  };
}

template<typename R, typename T, typename F>
auto decorate_shared_then(DirectContinuation<F, shared_future<T>>&& f) {
  return [f = std::forward<F>(f)](
    std::shared_ptr<shared_state<R>>&& state,
    std::shared_ptr<future_state<T>>&& parent
  ) mutable {
    set_state_value(*state, std::move(f), shared_future<T>{std::move(parent)});
  };
}

template<typename R, typename T, typename F>
auto decorate_shared_then(UnwrappableContinuation<F, shared_future<T>>&& f) {
  return [f = std::forward<F>(f)](
    std::shared_ptr<shared_state<R>>&& state,
    std::shared_ptr<future_state<T>>&& parent
  ) mutable {
    shared_state<R>::unwrap(state, state_of(this_ns::invoke(std::move(f), shared_future<T>{std::move(parent)})));
  };
}

template<typename R, typename T, typename F>
auto decorate_unique_next(DirectContinuation<F, T>&& f) {
  return [f = std::forward<F>(f)](
    std::shared_ptr<shared_state<R>>&& state,
    std::shared_ptr<future_state<T>>&& parent
  ) mutable {
    if (auto error = parent->exception()) {
      state->set_exception(std::move(error));
      return;
    }
    set_state_value(*state, std::move(f), std::move(parent->value_ref()));
  };
}

template<typename R, typename T, typename F>
auto decorate_unique_next(UnwrappableContinuation<F, T>&& f) {
  return [f = std::forward<F>(f)](
    std::shared_ptr<shared_state<R>>&& state,
    std::shared_ptr<future_state<T>>&& parent
  ) mutable {
    if (auto error = parent->exception()) {
      state->set_exception(std::move(error));
      return;
    }
    shared_state<R>::unwrap(state, state_of(this_ns::invoke(std::move(f), std::move(parent->value_ref()))));
  };
}

template<typename R, typename T, typename F>
auto decorate_shared_next(DirectContinuation<F, cref_t<T>>&& f) {
  return [f = std::forward<F>(f)](
    std::shared_ptr<shared_state<R>>&& state,
    std::shared_ptr<future_state<T>>&& parent
  ) mutable {
    if (auto error = parent->exception()) {
      state->set_exception(std::move(error));
      return;
    }
    set_state_value(*state, std::move(f), static_cast<cref_t<T>>(parent->value_ref()));
  };
}

template<typename R, typename T, typename F>
auto decorate_shared_next(UnwrappableContinuation<F, cref_t<T>>&& f) {
  return [f = std::forward<F>(f)](
    std::shared_ptr<shared_state<R>>&& state,
    std::shared_ptr<future_state<T>>&& parent
  ) mutable {
    if (auto error = parent->exception()) {
      state->set_exception(std::move(error));
      return;
    }
    shared_state<R>::unwrap(
      state, state_of(this_ns::invoke(std::move(f), static_cast<cref_t<T>>(parent->value_ref())))
    );
  };
}

template<typename R, typename F>
auto decorate_void_next(DirectContinuation<F, void>&& f) {
  return [f = std::forward<F>(f)](
    std::shared_ptr<shared_state<R>>&& state,
    std::shared_ptr<future_state<void>>&& parent
  ) mutable {
    if (auto error = parent->exception()) {
      state->set_exception(std::move(error));
      return;
    }
    set_state_value(*state, std::move(f));
  };
}

template<typename R, typename F>
auto decorate_void_next(UnwrappableContinuation<F, void>&& f) {
  return [f = std::forward<F>(f)](
    std::shared_ptr<shared_state<R>>&& state,
    std::shared_ptr<future_state<void>>&& parent
  ) mutable {
    if (auto error = parent->exception()) {
      state->set_exception(std::move(error));
      return;
    }
    shared_state<R>::unwrap(state, state_of(this_ns::invoke(std::move(f))));
  };
}

// continuation state

template<typename E, typename F, typename T>
struct cnt_closure {
  cnt_closure(const E& exec, F&& func, std::shared_ptr<future_state<T>> parent):
    exec(exec), func(std::move(func)), parent(std::move(parent))
  {}

  E exec;
  F func;
  std::shared_ptr<future_state<T>> parent;
};

class continuation_state {
public:
  virtual ~continuation_state() = default;
  virtual void run(std::shared_ptr<continuation_state> self_sp) = 0;
  virtual void abandon() = 0;
};

struct cnt_action {
  std::weak_ptr<continuation_state> wdata;

  cnt_action(const cnt_action&) = delete;

#if defined(__GNUC__) && __GNUC__ < 5
  cnt_action(std::weak_ptr<continuation_state> wdata): wdata(std::move(wdata)) {}
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
    if (auto data = wdata.lock())
      data->run(data);
    wdata.reset();
  }

  ~cnt_action() {
    if (auto data = wdata.lock())
      data->abandon();
  }
};

template<typename R, typename T, typename E, typename F>
class cnt_state final: public continuation_state {
public:
  cnt_state(const E& exec, F func, std::shared_ptr<future_state<T>>&& parent):
    action_(in_place_index_t<1>{}, exec, std::move(func), std::move(parent))
  {}

  future_state<R>* get_future_state() {return &state_;}

  void abandon() override {
    if (state_.continuations().executed())
      return;
    action_.clean();
    state_.set_exception(std::make_exception_ptr(std::future_error{std::future_errc::broken_promise}));
  }

  static void schedule(const std::shared_ptr<cnt_state>& self_sp) {
    E exec = self_sp->action_.get(in_place_index_t<1>{}).exec;
    post(exec, cnt_action{self_sp});
  }

  void run(std::shared_ptr<continuation_state> self_sp) override try {
    auto& action = action_.get(in_place_index_t<1>{});
    action.func(std::shared_ptr<shared_state<R>>{self_sp, &state_}, std::move(action.parent));
    action_.clean();
  } catch (...) {
    state_.set_exception(std::current_exception());
    action_.clean();
  }

private:
  shared_state<R> state_;
  either<detail::monostate, cnt_closure<E, F, T>> action_;
};

template<typename R, typename T, typename E, typename F>
auto make_then_state(std::shared_ptr<future_state<T>> parent, E&& exec, F&& f) {
  using cnt_data_t = cnt_state<R, T, std::decay_t<E>, std::decay_t<F>>;

  auto& continuations = parent->continuations();
  auto data = std::make_shared<cnt_data_t>(std::forward<E>(exec), std::forward<F>(f), std::move(parent));
  continuations.push([wdata = weak(data)] {
    if (auto data = wdata.lock())
      cnt_data_t::schedule(data);
  });
  return std::shared_ptr<future_state<R>>{data, data->get_future_state()};
}

} // namespace detail
} // inline namespace cxx14_v1
} // namespace portable_concurrency
