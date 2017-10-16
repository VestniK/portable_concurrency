#pragma once

#include "fwd.h"

#include "concurrency_type_traits.h"
#include "shared_state.h"
#include "utils.h"

namespace portable_concurrency {
inline namespace cxx14_v1 {
namespace detail {

template<typename T, typename R, typename F>
struct cnt_data {
  std::decay_t<F> func;
  std::shared_ptr<future_state<T>> parent;
  shared_state<R> state;

  cnt_data(F&& func, const std::shared_ptr<future_state<T>>& parent):
    func(std::forward<F>(func)), parent(parent)
  {}
};

template<typename Future, typename T, typename R, typename F>
auto cnt_run(std::shared_ptr<cnt_data<T, R, F>> data) ->
  std::enable_if_t<is_unique_future<std::result_of_t<F(Future)>>::value>
{
  auto res = ::portable_concurrency::cxx14_v1::detail::invoke(std::move(data->func), std::move(data->parent));
  if (!res.valid()) {
    shared_state<R>::set_exception(
      std::shared_ptr<shared_state<R>>{data, &data->state},
      std::make_exception_ptr(std::future_error{std::future_errc::broken_promise})
    );
    return;
  }
  using res_t = std::result_of_t<F(Future)>;
  auto& continuations = state_of(res)->continuations();
  continuations.push([data = std::move(data), res = std::move(res)] () mutable {
    set_state_value(
      std::shared_ptr<shared_state<R>>{data, &data->state},
      &res_t::get, std::move(res)
    );
  });
}

template<typename Future, typename T, typename R, typename F>
auto cnt_run(std::shared_ptr<cnt_data<T, R, F>> data) ->
  std::enable_if_t<!is_unique_future<std::result_of_t<F(Future)>>::value>
{
  set_state_value(
    std::shared_ptr<shared_state<R>>(data, &data->state),
    std::move(data->func), Future{std::move(data->parent)}
  );
}

template<template<typename> class Future, typename T, typename F>
auto make_then_state(std::shared_ptr<future_state<T>> parent, F&& f) {
  using CntRes = continuation_result_t<Future, F, T>;
  using R = remove_future_t<CntRes>;
  if (!parent)
    throw std::future_error(std::future_errc::no_state);
  auto data = std::make_shared<cnt_data<T, R, F>>(
    std::forward<F>(f), std::move(parent)
  );
  data->parent->continuations().push([data]{cnt_run<Future<T>>(std::move(data));});
  return std::shared_ptr<future_state<R>>{data, &data->state};
}

} // namespace detail
} // inline namespace cxx14_v1
} // namespace portable_concurrency
