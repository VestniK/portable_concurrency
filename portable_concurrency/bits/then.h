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

template<template<typename> class Future, typename T, typename F>
std::shared_ptr<future_state<continuation_result_t<Future, F, T>>>
make_then_state(std::shared_ptr<future_state<T>> parent, F&& f) {
  using R = continuation_result_t<Future, F, T>;
  if (!parent)
    throw std::future_error(std::future_errc::no_state);
  auto data = std::make_shared<cnt_data<T, R, F>>(
    std::forward<F>(f), std::move(parent)
  );
  auto res = std::shared_ptr<shared_state<R>>(data, &data->state);
  data->parent->continuations().push([data]{
    set_state_value(
      std::shared_ptr<shared_state<R>>(data, &data->state),
      std::move(data->func), Future<T>{std::move(data->parent)}
    );
  });
  return {data, &data->state};
}

} // namespace detail
} // inline namespace cxx14_v1
} // namespace portable_concurrency
