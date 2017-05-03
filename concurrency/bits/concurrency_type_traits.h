#pragma once

#include <type_traits>

namespace experimental {
inline namespace concurrency_v1 {
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

} // namespace detail
} // inline namespace concurrency_v1
} // namespace experimental
