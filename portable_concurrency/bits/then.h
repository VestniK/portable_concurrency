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

template<typename... T>
struct voidify {using type = void;};

template<typename... T>
using void_t = typename voidify<T...>::type;

template<typename F, typename S, typename = void>
struct is_invocable_s: std::false_type {};

template<typename F, typename R, typename... A>
struct is_invocable_s<F, R(A...), void_t<decltype(this_ns::invoke(std::declval<F>(), std::declval<A>()...))>>:
  std::is_same<R, decltype(this_ns::invoke(std::declval<F>(), std::declval<A>()...))>
{};

template<typename F, typename S>
using Callable = std::enable_if_t<is_invocable_s<F, S>::value, F>;

template<typename R, typename T, typename F>
auto decorate_unique_then(Callable<F, R(future<T>)>&& f) {
  return [f = std::forward<F>(f)](
    std::shared_ptr<shared_state<R>>&& state,
    std::shared_ptr<future_state<T>>&& parent
  ) mutable {
    set_state_value(*state, std::move(f), future<T>{std::move(parent)});
  };
}

template<typename R, typename T, typename F>
auto decorate_unique_then(Callable<F, future<R>(future<T>)>&& f) {
  return [f = std::forward<F>(f)](
    std::shared_ptr<shared_state<R>>&& state,
    std::shared_ptr<future_state<T>>&& parent
  ) mutable {
    shared_state<R>::unwrap(state, state_of(this_ns::invoke(std::move(f), future<T>{std::move(parent)})));
  };
}

template<typename R, typename T, typename F>
auto decorate_unique_then(Callable<F, shared_future<R>(future<T>)>&& f) {
  return [f = std::forward<F>(f)](
    std::shared_ptr<shared_state<R>>&& state,
    std::shared_ptr<future_state<T>>&& parent
  ) mutable {
    shared_state<R>::unwrap(state, state_of(this_ns::invoke(std::move(f), future<T>{std::move(parent)})));
  };
}

template<typename R, typename T, typename F>
auto decorate_shared_then(Callable<F, R(shared_future<T>)>&& f) {
  return [f = std::forward<F>(f)](
    std::shared_ptr<shared_state<R>>&& state,
    std::shared_ptr<future_state<T>>&& parent
  ) mutable {
    set_state_value(*state, std::move(f), shared_future<T>{std::move(parent)});
  };
}

template<typename R, typename T, typename F>
auto decorate_shared_then(Callable<F, future<R>(shared_future<T>)>&& f) {
  return [f = std::forward<F>(f)](
    std::shared_ptr<shared_state<R>>&& state,
    std::shared_ptr<future_state<T>>&& parent
  ) mutable {
    shared_state<R>::unwrap(state, state_of(this_ns::invoke(std::move(f), shared_future<T>{std::move(parent)})));
  };
}

template<typename R, typename T, typename F>
auto decorate_shared_then(Callable<F, shared_future<R>(shared_future<T>)>&& f) {
  return [f = std::forward<F>(f)](
    std::shared_ptr<shared_state<R>>&& state,
    std::shared_ptr<future_state<T>>&& parent
  ) mutable {
    shared_state<R>::unwrap(state, state_of(this_ns::invoke(std::move(f), shared_future<T>{std::move(parent)})));
  };
}

// Tags for different continuation types and helper type traits to work with them

enum class cnt_tag {
  next,
  shared_next
};

template<cnt_tag Type, typename T>
struct cnt_arg;
template<cnt_tag Type, typename T>
using cnt_arg_t = typename cnt_arg<Type, T>::type;

template<typename T>
struct cnt_arg<cnt_tag::next, T> {
  using type = T;
  static type&& extract(std::shared_ptr<future_state<T>>& state) {return std::move(state->value_ref());}
};
template<>
struct cnt_arg<cnt_tag::next, void> {
  using type = void;
  static void extract(std::shared_ptr<future_state<void>>& state) {state->value_ref();}
};
template<typename T>
struct cnt_arg<cnt_tag::shared_next, T> {
  using type = std::add_lvalue_reference_t<std::add_const_t<T>>;
  static const type& extract(std::shared_ptr<future_state<T>>& state) {return state->value_ref();}
};
template<>
struct cnt_arg<cnt_tag::shared_next, void> {
  using type = void;
  static void extract(std::shared_ptr<future_state<void>>& state) {state->value_ref();}
};

// Invoke continuation and emplace result into storage. Separate helpers for cases when continuation argument is
// void/non-void and continuation result is void/non-void.

template<typename T>
using is_regular = std::integral_constant<bool, !std::is_void<T>::value && !is_future<T>::value>;

template<cnt_tag Tag, typename S, typename F, typename T>
auto invoke_emplace(std::shared_ptr<S>&& state_sp, F&& func, std::shared_ptr<future_state<T>>) -> std::enable_if_t<
  std::is_void<cnt_arg_t<Tag, T>>::value && is_regular<
    decltype(portable_concurrency::detail::invoke(std::forward<F>(func)))
  >::value
> {
  state_sp->emplace(portable_concurrency::detail::invoke(std::forward<F>(func)));
}

template<cnt_tag Tag, typename S, typename F, typename T>
auto invoke_emplace(std::shared_ptr<S>&& state_sp, F&& func, std::shared_ptr<future_state<T>>) -> std::enable_if_t<
  std::is_void<cnt_arg_t<Tag, T>>::value && std::is_void<
    decltype(portable_concurrency::detail::invoke(std::forward<F>(func)))
  >::value
> {
  portable_concurrency::detail::invoke(std::forward<F>(func));
  state_sp->emplace();
}

template<cnt_tag Tag, typename S, typename F, typename T>
auto invoke_emplace(std::shared_ptr<S>&& state_sp, F&& func, std::shared_ptr<future_state<T>>) -> std::enable_if_t<
  std::is_void<cnt_arg_t<Tag, T>>::value && is_future<
    decltype(portable_concurrency::detail::invoke(std::forward<F>(func)))
  >::value
> {
  S::unwrap(state_sp, state_of(portable_concurrency::detail::invoke(std::forward<F>(func))));
}

template<cnt_tag Tag, typename S, typename F, typename T>
auto invoke_emplace(std::shared_ptr<S>&& state_sp, F&& func, std::shared_ptr<future_state<T>> parent) -> std::enable_if_t<
  !std::is_void<cnt_arg_t<Tag, T>>::value && is_regular<
    decltype(portable_concurrency::detail::invoke(std::forward<F>(func), cnt_arg<Tag, T>::extract(parent)))
  >::value
> {
  state_sp->emplace(
    portable_concurrency::detail::invoke(std::forward<F>(func), cnt_arg<Tag, T>::extract(parent))
  );
}

template<cnt_tag Tag, typename S, typename F, typename T>
auto invoke_emplace(std::shared_ptr<S>&& state_sp, F&& func, std::shared_ptr<future_state<T>> parent) -> std::enable_if_t<
  !std::is_void<cnt_arg_t<Tag, T>>::value && std::is_void<
    decltype(portable_concurrency::detail::invoke(std::forward<F>(func), cnt_arg<Tag, T>::extract(parent)))
  >::value
> {
  portable_concurrency::detail::invoke(std::forward<F>(func), cnt_arg<Tag, T>::extract(parent));
  state_sp->emplace();
}

template<cnt_tag Tag, typename S, typename F, typename T>
auto invoke_emplace(std::shared_ptr<S>&& state_sp, F&& func, std::shared_ptr<future_state<T>> parent) -> std::enable_if_t<
  !std::is_void<cnt_arg_t<Tag, T>>::value && is_future<
    decltype(portable_concurrency::detail::invoke(std::forward<F>(func), cnt_arg<Tag, T>::extract(parent)))
  >::value
> {
  S::unwrap(state_sp, state_of(
    portable_concurrency::detail::invoke(std::forward<F>(func), cnt_arg<Tag, T>::extract(parent))
  ));
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

template<cnt_tag Tag, typename T, typename F>
auto decorate_next(F&& f) {
  using cnt_res = cnt_result_t<F, cnt_arg_t<Tag, T>>;
  using value_type = remove_future_t<cnt_res>;

  return [func = std::decay_t<F>{std::forward<F>(f)}](
    std::shared_ptr<shared_state<value_type>> state, std::shared_ptr<future_state<T>> parent
  ) mutable {
    if (auto error = parent->exception()) {
      state->set_exception(std::move(error));
      return;
    }
    invoke_emplace<Tag>(std::move(state), std::move(func), std::move(parent));
  };
}

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
