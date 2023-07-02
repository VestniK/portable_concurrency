#pragma once

#include "fwd.h"

#include "concurrency_type_traits.h"
#include "execution.h"
#include "shared_state.h"
#include "utils.h"

namespace portable_concurrency {
inline namespace cxx14_v1 {
namespace detail {

// Decorate different functors in order to be callable as
// `decorated_func(shared_ptr<shared_state<R>>, shared_ptr<future_state<T>>);`

namespace this_ns = ::portable_concurrency::cxx14_v1::detail;

template <typename T>
using cref_t = std::add_lvalue_reference_t<std::add_const_t<T>>;

template <typename F, typename T>
using DirectContinuation =
    std::enable_if_t<!is_future<cnt_result_t<F, T>>::value, F>;

template <typename F, typename T>
using UnwrappableContinuation =
    std::enable_if_t<is_future<cnt_result_t<F, T>>::value, F>;

template <typename R, typename T, typename F>
auto decorate_unique_then(DirectContinuation<F, future<T>> &&f,
                          std::shared_ptr<future_state<T>> &&parent) {
  return [f = std::forward<F>(f), parent = std::move(parent)](
             std::shared_ptr<shared_state<R>> &&state) mutable {
    set_state_value(state, std::move(f), future<T>{std::move(parent)});
  };
}

template <typename R, typename T, typename F>
auto decorate_unique_then(UnwrappableContinuation<F, future<T>> &&f,
                          std::shared_ptr<future_state<T>> &&parent) {
  return [f = std::forward<F>(f), parent = std::move(parent)](
             std::shared_ptr<shared_state<R>> &&state) mutable {
    try {
      shared_state<R>::unwrap(
          state, state_of(this_ns::invoke(std::move(f),
                                          future<T>{std::move(parent)})));
    } catch (...) {
      state->set_exception(std::current_exception());
    }
  };
}

template <typename R, typename T, typename F>
auto decorate_shared_then(DirectContinuation<F, shared_future<T>> &&f,
                          const std::shared_ptr<future_state<T>> &parent) {
  return [f = std::forward<F>(f),
          parent = std::shared_ptr<future_state<T>>{parent}](
             std::shared_ptr<shared_state<R>> &&state) mutable {
    set_state_value(state, std::move(f), shared_future<T>{std::move(parent)});
  };
}

template <typename R, typename T, typename F>
auto decorate_shared_then(UnwrappableContinuation<F, shared_future<T>> &&f,
                          const std::shared_ptr<future_state<T>> &parent) {
  return [f = std::forward<F>(f),
          parent = std::shared_ptr<future_state<T>>{parent}](
             std::shared_ptr<shared_state<R>> &&state) mutable {
    try {
      shared_state<R>::unwrap(
          state, state_of(this_ns::invoke(
                     std::move(f), shared_future<T>{std::move(parent)})));
    } catch (...) {
      state->set_exception(std::current_exception());
    }
  };
}

template <typename R, typename T, typename F>
auto decorate_unique_next(DirectContinuation<F, T> &&f,
                          std::shared_ptr<future_state<T>> &&parent) {
  return [f = std::forward<F>(f), parent = std::move(parent)](
             std::shared_ptr<shared_state<R>> &&state) mutable {
    if (auto error = parent->exception()) {
      state->set_exception(std::move(error));
      return;
    }
    set_state_value(state, std::move(f), std::move(parent->value_ref()));
  };
}

template <typename R, typename T, typename F>
auto decorate_unique_next(UnwrappableContinuation<F, T> &&f,
                          std::shared_ptr<future_state<T>> &&parent) {
  return [f = std::forward<F>(f), parent = std::move(parent)](
             std::shared_ptr<shared_state<R>> &&state) mutable {
    if (auto error = parent->exception()) {
      state->set_exception(std::move(error));
      return;
    }
    try {
      shared_state<R>::unwrap(
          state, state_of(this_ns::invoke(std::move(f),
                                          std::move(parent->value_ref()))));
    } catch (...) {
      state->set_exception(std::current_exception());
    }
  };
}

template <typename R, typename T, typename F>
auto decorate_shared_next(DirectContinuation<F, cref_t<T>> &&f,
                          const std::shared_ptr<future_state<T>> &parent) {
  return [f = std::forward<F>(f),
          parent](std::shared_ptr<shared_state<R>> &&state) mutable {
    if (auto error = parent->exception()) {
      state->set_exception(std::move(error));
      return;
    }
    set_state_value(state, std::move(f),
                    static_cast<cref_t<T>>(parent->value_ref()));
  };
}

template <typename R, typename T, typename F>
auto decorate_shared_next(UnwrappableContinuation<F, cref_t<T>> &&f,
                          const std::shared_ptr<future_state<T>> &parent) {
  return [f = std::forward<F>(f),
          parent](std::shared_ptr<shared_state<R>> &&state) mutable {
    if (auto error = parent->exception()) {
      state->set_exception(std::move(error));
      return;
    }
    try {
      shared_state<R>::unwrap(
          state,
          state_of(this_ns::invoke(
              std::move(f), static_cast<cref_t<T>>(parent->value_ref()))));
    } catch (...) {
      state->set_exception(std::current_exception());
    }
  };
}

template <typename R, typename F>
auto decorate_void_next(DirectContinuation<F, void> &&f,
                        std::shared_ptr<future_state<void>> parent) {
  return [f = std::forward<F>(f), parent = std::move(parent)](
             std::shared_ptr<shared_state<R>> &&state) mutable {
    if (auto error = parent->exception()) {
      state->set_exception(std::move(error));
      return;
    }
    set_state_value(state, std::move(f));
  };
}

template <typename R, typename F>
auto decorate_void_next(UnwrappableContinuation<F, void> &&f,
                        std::shared_ptr<future_state<void>> parent) {
  return [f = std::forward<F>(f), parent = std::move(parent)](
             std::shared_ptr<shared_state<R>> &&state) mutable {
    if (auto error = parent->exception()) {
      state->set_exception(std::move(error));
      return;
    }
    try {
      shared_state<R>::unwrap(state, state_of(this_ns::invoke(std::move(f))));
    } catch (...) {
      state->set_exception(std::current_exception());
    }
  };
}

// continuation state

template <typename CntState> struct cnt_action {
  std::weak_ptr<CntState> wdata;

  cnt_action(const cnt_action &) = delete;
  cnt_action &operator=(const cnt_action &) = delete;

  cnt_action(std::weak_ptr<CntState> wdata) : wdata(std::move(wdata)) {}
  cnt_action(cnt_action &&rhs) noexcept : wdata(std::move(rhs.wdata)) {}
  cnt_action &operator=(cnt_action &&rhs) noexcept {
    wdata = std::move(rhs.wdata);
    return *this;
  }

  void operator()() { CntState::run(std::exchange(wdata, {})); }

  ~cnt_action() {
    if (auto data = wdata.lock())
      data->abandon();
  }
};

template <typename R, typename F, typename E> struct cnt_state {
  void abandon() {
    state.abandon();
    action.clean();
  }

  static void run(std::weak_ptr<cnt_state> wself) noexcept {
    std::shared_ptr<cnt_state> self = wself.lock();
    if (!self)
      return;
    F func = std::move(self->action.get(in_place_index_t<1>{}));
    self->action.clean();
    shared_state<R> *state = &self->state;
    std::shared_ptr<shared_state<R>> state_sp{std::exchange(self, nullptr),
                                              state};
    func(std::move(state_sp));
  }

  static void schedule(std::weak_ptr<cnt_state> wself) {
    auto self = wself.lock();
    if (!self)
      return;
    E exec = std::move(self->exec.get(in_place_index_t<1>{}));
    self->exec.clean();
    std::weak_ptr<cnt_state> wstate = std::exchange(self, nullptr);
    post(exec, cnt_action<cnt_state>{std::move(wstate)});
  }

  either<detail::monostate, E> exec;
  either<detail::monostate, F> action;
  shared_state<R> state;
};

template <typename R, typename E, typename F>
auto make_then_state(continuations_stack &subscriptions, E &&exec, F &&f) {
  using cnt_data_t = cnt_state<R, std::decay_t<F>, std::decay_t<E>>;

  auto data = std::make_shared<cnt_data_t>();
  data->exec.emplace(in_place_index_t<1>{}, std::forward<E>(exec));
  data->action.emplace(in_place_index_t<1>{}, std::forward<F>(f));
  subscriptions.push([wdata = std::weak_ptr<cnt_data_t>{data}] {
    cnt_data_t::schedule(wdata);
  });
  return std::shared_ptr<future_state<R>>{data, &data->state};
}

} // namespace detail
} // namespace cxx14_v1
} // namespace portable_concurrency
