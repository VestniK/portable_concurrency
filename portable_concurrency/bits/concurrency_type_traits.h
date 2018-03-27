#pragma once

#include <type_traits>

#include "fwd.h"

namespace portable_concurrency {
inline namespace cxx14_v1 {
namespace detail {

template<template<typename...> class T, typename U>
struct is_instantiation_of: std::false_type {};
template<template<typename...> class T, typename... U>
struct is_instantiation_of<T, T<U...>>: std::true_type {};

// is_future

template<typename T>
using is_unique_future = std::integral_constant<bool, is_instantiation_of<future, T>::value>;
template<typename T>
using is_shared_future = std::integral_constant<bool, is_instantiation_of<shared_future, T>::value>;
template<typename T>
using is_future = std::integral_constant<bool, is_unique_future<T>::value || is_shared_future<T>::value>;

// are_futures

template<typename... F>
struct are_futures;

template<>
struct are_futures<>: std::true_type {};

template<typename F0, typename... F>
struct are_futures<F0, F...>: std::integral_constant<
  bool,
  is_future<F0>::value && are_futures<F...>::value
> {};

// remove_future

template<typename T>
struct remove_future {using type = T;};

template<typename T>
struct remove_future<future<T>> {using type = T;};

template<typename T>
struct remove_future<shared_future<T>> {using type = T;};

template<typename T>
using remove_future_t = typename remove_future<T>::type;

// add_future

template<typename T> struct add_future {using type = future<T>;};
template<typename T> struct add_future<future<T>> {using type = future<T>;};
template<typename T> struct add_future<shared_future<T>> {using type = shared_future<T>;};

template<typename T>
using add_future_t = typename add_future<T>::type;

// cnt_future_t

template<typename Func, typename Arg>
struct cnt_result: std::result_of<Func(Arg)> {};

template<typename Func>
struct cnt_result<Func, void>: std::result_of<Func()> {};

template<typename Func, typename Arg>
using cnt_result_t = typename cnt_result<Func, Arg>::type;

template<typename Func, typename Arg>
using cnt_future_t = add_future_t<cnt_result_t<Func, Arg>>;

// deduce result type for interruptable continuation

template<typename T>
struct promise_arg {
  template<typename R>
  static R deduce(void (*) (promise<R>, future<T>));

  template<typename R, typename C>
  static R deduce_method(void (C::*) (promise<R>, future<T>) const);

  template<typename R, typename C>
  static R deduce_method(void (C::*) (promise<R>, future<T>));

  template<typename F>
  static auto deduce(F) -> decltype(deduce_method(&F::operator()));
};

template<typename F, typename T>
using promise_arg_t = decltype(promise_arg<T>::deduce(std::declval<std::decay_t<F>>()));

// varaidic helper swallow

struct swallow {
  swallow(...) {}
};

} // namespace detail
} // inline namespace cxx14_v1
} // namespace portable_concurrency
