#pragma once

#include <type_traits>

#include "future.h"
#include "promise.h"

namespace portable_concurrency {
inline namespace cxx14_v1 {
namespace detail {

template <typename T> struct decay_for_future : std::decay<T> {};

template <typename T> struct decay_for_future<std::reference_wrapper<T>> {
  using type = T &;
};

template <typename T>
using decay_for_future_t = typename decay_for_future<T>::type;

} // namespace detail

template <typename T>
future<detail::decay_for_future_t<T>> make_ready_future(T &&value) {
  auto promise_and_future = make_promise<detail::decay_for_future_t<T>>();
  promise_and_future.first.set_value(std::forward<T>(value));
  return std::move(promise_and_future.second);
}

future<void> make_ready_future();

template <typename T>
future<T> make_exceptional_future(std::exception_ptr error) {
  promise<T> promise;
  promise.set_exception(error);
  return promise.get_future();
}

template <typename T, typename E> future<T> make_exceptional_future(E error) {
  promise<T> promise;
  promise.set_exception(std::make_exception_ptr(error));
  return promise.get_future();
}

} // namespace cxx14_v1
} // namespace portable_concurrency
