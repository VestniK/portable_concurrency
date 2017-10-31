#pragma once

#include <type_traits>

#include "fwd.h"

namespace portable_concurrency {
inline namespace cxx14_v1 {
namespace detail {

template<typename F>
struct is_unique_future: std::false_type {};

template<typename T>
struct is_unique_future<future<T>>: std::true_type {};

template<typename F>
struct is_shared_future: std::false_type {};

template<typename T>
struct is_shared_future<shared_future<T>>: std::true_type {};

template<typename F>
using is_future = std::integral_constant<bool, is_unique_future<F>::value || is_shared_future<F>::value>;

template<typename... F>
struct are_futures;

template<>
struct are_futures<>: std::true_type {};

template<typename F0, typename... F>
struct are_futures<F0, F...>: std::integral_constant<
  bool,
  is_future<F0>::value && are_futures<F...>::value
> {};

template<typename T>
struct remove_future {using type = T;};

template<typename T>
struct remove_future<future<T>> {using type = T;};

template<typename T>
using remove_future_t = typename remove_future<T>::type;

template<typename Func, typename Arg>
struct cnt_result: std::result_of<Func(Arg)> {};

template<typename Func>
struct cnt_result<Func, void>: std::result_of<Func()> {};

template<typename Func, typename Arg>
using cnt_result_t = typename cnt_result<Func, Arg>::type;

template<typename Func, typename Arg>
using cnt_future_t = future<remove_future_t<cnt_result_t<Func, Arg>>>;

} // namespace detail
} // inline namespace cxx14_v1
} // namespace portable_concurrency
