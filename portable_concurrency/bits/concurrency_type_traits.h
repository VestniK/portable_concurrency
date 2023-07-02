#pragma once

#include <type_traits>

#include "fwd.h"
#include "voidify.h"

namespace portable_concurrency {
inline namespace cxx14_v1 {
namespace detail {

#if defined(__cpp_lib_is_invocable)
template <typename F, typename... A>
using invoke_result = std::invoke_result<F, A...>;
#else
template <typename F, typename... A>
using invoke_result = std::result_of<F(A...)>;
#endif
template <typename F, typename... A>
using invoke_result_t = typename invoke_result<F, A...>::type;

template <template <typename...> class T, typename U>
struct is_instantiation_of : std::false_type {};
template <template <typename...> class T, typename... U>
struct is_instantiation_of<T, T<U...>> : std::true_type {};

// is_future

template <typename T>
using is_unique_future =
    std::integral_constant<bool, is_instantiation_of<future, T>::value>;
template <typename T>
using is_shared_future =
    std::integral_constant<bool, is_instantiation_of<shared_future, T>::value>;
template <typename T>
using is_future = std::integral_constant<bool, is_unique_future<T>::value ||
                                                   is_shared_future<T>::value>;

// are_futures

template <typename... F> struct are_futures;

template <> struct are_futures<> : std::true_type {};

template <typename F0, typename... F>
struct are_futures<F0, F...>
    : std::integral_constant<bool, is_future<F0>::value &&
                                       are_futures<F...>::value> {};

// remove_future

template <typename T> struct remove_future {
  using type = T;
};

template <typename T> struct remove_future<future<T>> {
  using type = T;
};

template <typename T> struct remove_future<shared_future<T>> {
  using type = T;
};

template <typename T> using remove_future_t = typename remove_future<T>::type;

// add_future

template <typename T> struct add_future {
  using type = future<T>;
};
template <typename T> struct add_future<future<T>> {
  using type = future<T>;
};
template <typename T> struct add_future<shared_future<T>> {
  using type = shared_future<T>;
};

template <typename T> using add_future_t = typename add_future<T>::type;

// cnt_future_t

template <typename Func, typename Arg>
struct cnt_result : invoke_result<Func, Arg> {};

template <typename Func> struct cnt_result<Func, void> : invoke_result<Func> {};

template <typename Func, typename Arg>
using cnt_result_t = typename cnt_result<Func, Arg>::type;

template <typename Func, typename Arg>
using cnt_future_t = add_future_t<cnt_result_t<Func, Arg>>;

// deduce result type for interruptible continuation

template <typename Future> struct promise_deducer {
  static_assert(is_future<Future>::value,
                "Future parameter must be future<T> or shared_future<T>");

  template <typename R> static R deduce(void (*)(promise<R>, Future));

  template <typename R, typename C>
  static R deduce_method(void (C::*)(promise<R>, Future) const);

  template <typename R, typename C>
  static R deduce_method(void (C::*)(promise<R>, Future));

  template <typename F>
  static auto deduce(F) -> decltype(deduce_method(&F::operator()));
};

template <typename F, typename T, typename = void> struct promise_arg {};

template <typename Func, typename Future>
struct promise_arg<Func, Future,
                   typename voidify<decltype(promise_deducer<Future>::deduce(
                       std::declval<std::decay_t<Func>>()))>::type> {
  using type = decltype(promise_deducer<Future>::deduce(
      std::declval<std::decay_t<Func>>()));
};

template <typename Func, typename Future>
using promise_arg_t = typename promise_arg<Func, Future>::type;

// variadic helper swallow
struct swallow {
  swallow(...) {}
};

} // namespace detail
} // namespace cxx14_v1
} // namespace portable_concurrency
