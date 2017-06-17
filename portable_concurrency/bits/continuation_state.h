#pragma once

#include "fwd.h"

#include "concurrency_type_traits.h"
#include "continuation.h"
#include "shared_state.h"
#include "utils.h"

namespace portable_concurrency {
inline namespace cxx14_v1 {
namespace detail {

template<template<typename> class Future, typename Func, typename T>
using continuation_result_t = std::result_of_t<Func(Future<T>)>;

template<template<typename> class Future, typename F, typename T>
class continuation_state:
  public shared_state<continuation_result_t<Future, F, T>>,
  public continuation
{
 public:
  using result_t = continuation_result_t<Future, F, T>;

  continuation_state(F&& f, std::shared_ptr<future_state<T>>&& parent):
    func_(std::forward<F>(f)),
    parent_(std::move(parent))
  {
  }

  static std::shared_ptr<shared_state<continuation_result_t<Future, F, T>>> make(
    F&& func,
    std::shared_ptr<future_state<T>> parent
  ) {
    auto res = std::make_shared<continuation_state>(std::forward<F>(func), std::move(parent));
    res->parent_->add_continuation(res);
    return res;
  }

  void invoke() override {
    ::portable_concurrency::cxx14_v1::detail::set_state_value(
      *this,
      std::move(func_),
      Future<T>(std::move(parent_))
    );
  }

private:
  std::decay_t<F> func_;
  std::shared_ptr<future_state<T>> parent_;
 };

 } // namespace detail
 } // inline namespace cxx14_v1
 } // namespace portable_concurrency
