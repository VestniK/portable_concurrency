#pragma once

#include "fwd.h"

#include "concurrency_type_traits.h"
#include "shared_state.h"
#include "utils.h"

namespace portable_concurrency {
inline namespace cxx14_v1 {
namespace detail {

template<typename T, typename R, typename F, typename E>
struct cnt_data;

template<typename T, typename R, typename F>
struct cnt_data<T, R, F, void> {
  F func;
  std::shared_ptr<future_state<T>> parent;
  shared_state<R> state;

  cnt_data(F func, std::shared_ptr<future_state<T>> parent):
    func(std::move(func)), parent(std::move(parent))
  {}
};

template<typename T, typename R, typename F, typename E>
struct cnt_data: cnt_data<T, R, F, void> {
  E exec;

  cnt_data(E exec, F func, std::shared_ptr<future_state<T>> parent):
    cnt_data<T, R, F, void>(std::move(func), std::move(parent)),
    exec(std::move(exec))
  {}
};

template<typename T, typename R, typename F, typename E>
struct cnt_data_holder {
  std::weak_ptr<cnt_data<T, R, F, E>> data;

  cnt_data_holder(const cnt_data_holder&) = delete;

  cnt_data_holder(cnt_data_holder&&) noexcept = default;
  cnt_data_holder& operator=(cnt_data_holder&&) noexcept = default;

  ~cnt_data_holder() {
    auto sdata = data.lock();
    if (sdata && !sdata->state.continuations().executed())
      sdata->state.set_exception(std::make_exception_ptr(std::future_error{std::future_errc::broken_promise}));
  }
};

template<typename Future, typename T, typename R, typename F, typename E>
auto then_run(std::weak_ptr<cnt_data<T, R, F, E>> wdata) -> std::enable_if_t<
  is_unique_future<std::result_of_t<F(Future)>>::value
> {
  auto data = wdata.lock();
  if (!data)
    return;

  using res_t = std::result_of_t<F(Future)>;
  res_t res;
  try {
    res = ::portable_concurrency::cxx14_v1::detail::invoke(std::move(data->func), std::move(data->parent));
  } catch(...) {
    data->state.set_exception(std::current_exception());
    return;
  }
  if (!res.valid()) {
    data->state.set_exception(std::make_exception_ptr(std::future_error{std::future_errc::broken_promise}));
    return;
  }
  auto& continuations = state_of(res)->continuations();
  continuations.push([data = std::move(data), res = std::move(res)] () mutable {
    set_state_value(data->state, &res_t::get, std::move(res));
  });
}

template<typename Future, typename T, typename R, typename F, typename E>
auto then_run(std::weak_ptr<cnt_data<T, R, F, E>> wdata) -> std::enable_if_t<
  !is_unique_future<std::result_of_t<F(Future)>>::value
> {
  if (auto data = wdata.lock())
    set_state_value(data->state, std::move(data->func), Future{std::move(data->parent)});
}

template<typename Future, typename T, typename R, typename F, typename E>
void then_post(std::weak_ptr<cnt_data<T, R, F, E>> wdata) {
  auto data = wdata.lock();
  if (!data)
    return;

  post(std::move(data->exec), [data = cnt_data_holder<T, R, F, E>{std::move(wdata)}]() mutable {
    then_run<Future>(std::move(data.data));
  });
}

template<template<typename> class Future, typename T, typename F>
auto make_then_state(std::shared_ptr<future_state<T>> parent, F&& f) {
  using CntRes = then_result_t<Future, F, T>;
  using R = remove_future_t<CntRes>;

  auto data = std::make_shared<cnt_data<T, R, std::decay_t<F>, void>>(
    std::forward<F>(f), std::move(parent)
  );
  data->parent->continuations().push([wdata = std::weak_ptr<cnt_data<T, R, std::decay_t<F>, void>>(data)]() mutable {
    then_run<Future<T>>(std::move(wdata));
  });
  return std::shared_ptr<future_state<R>>{data, &data->state};
}

template<template<typename> class Future, typename T, typename E, typename F>
auto make_then_state(std::shared_ptr<future_state<T>> parent, E&& exec, F&& f) {
  using CntRes = then_result_t<Future, F, T>;
  using R = remove_future_t<CntRes>;

  auto data = std::make_shared<cnt_data<T, R, std::decay_t<F>, std::decay_t<E>>>(
    std::forward<E>(exec), std::forward<F>(f), std::move(parent)
  );
  data->parent->continuations().push([wdata = std::weak_ptr<cnt_data<T, R, std::decay_t<F>, std::decay_t<E>>>(data)]() mutable {
    then_post<Future<T>>(std::move(wdata));
  });
  return std::shared_ptr<future_state<R>>{data, &data->state};
}

template<typename T, typename R, typename F, typename E>
void next_run(std::weak_ptr<cnt_data<T, R, F, E>> wdata) {
  auto data = wdata.lock();
  if (!data)
    return;

  if (auto error = data->parent->exception()) {
    data->state.set_exception(std::move(error));
    return;
  }
  set_state_value(data->state, std::move(data->func), std::move(data->parent->value_ref()));
}

template<typename R, typename F, typename E>
void next_run(std::weak_ptr<cnt_data<void, R, F, E>> wdata) {
  auto data = wdata.lock();
  if (!data)
    return;

  if (auto error = data->parent->exception()) {
    data->state.set_exception(std::move(error));
    return;
  }
  set_state_value(data->state, std::move(data->func));
}

template<typename T, typename R, typename F, typename E>
void next_post(std::weak_ptr<cnt_data<T, R, F, E>> wdata) {
  auto data = wdata.lock();
  if (!data)
    return;

  post(std::move(data->exec), [data = cnt_data_holder<T, R, F, E>{std::move(wdata)}]() mutable {
    next_run(std::move(data.data));
  });
}

template<typename T, typename F>
auto make_next_state(std::shared_ptr<future_state<T>> parent, F&& f) {
  using CntRes = next_result_t<F, T>;
  using R = remove_future_t<CntRes>;

  auto data = std::make_shared<cnt_data<T, R, std::decay_t<F>, void>>(
    std::forward<F>(f), std::move(parent)
  );
  data->parent->continuations().push([data = std::weak_ptr<cnt_data<T, R, std::decay_t<F>, void>>{data}]() mutable {
    next_run(std::move(data));
  });
  return std::shared_ptr<future_state<R>>{data, &data->state};
}

template<typename T, typename E, typename F>
auto make_next_state(std::shared_ptr<future_state<T>> parent, E&& exec, F&& f) {
  using CntRes = next_result_t<F, T>;
  using R = remove_future_t<CntRes>;

  auto data = std::make_shared<cnt_data<T, R, std::decay_t<F>, std::decay_t<E>>>(
    std::forward<E>(exec), std::forward<F>(f), std::move(parent)
  );
  data->parent->continuations().push([data = std::weak_ptr<cnt_data<T, R, std::decay_t<F>, std::decay_t<E>>>{data}]() mutable {
    next_post(std::move(data));
  });
  return std::shared_ptr<future_state<R>>{data, &data->state};
}

} // namespace detail
} // inline namespace cxx14_v1
} // namespace portable_concurrency
