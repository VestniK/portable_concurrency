#pragma once

#include <tuple>
#include <utility>

#include "concurrency_type_traits.h"
#include "execution.h"
#include "invoke.h"
#include "packaged_task.h"
#include "shared_state.h"
#include "then.hpp"

#include <portable_concurrency/bits/config.h>

namespace portable_concurrency {
inline namespace cxx14_v1 {

namespace detail {

namespace this_ns = portable_concurrency::cxx14_v1::detail;

template <typename F, typename... A> class task {
public:
  task(F &&f, A &&...a)
      : func_(std::forward<F>(f)), args_(std::forward<A>(a)...) {}

  auto operator()() { return run(std::make_index_sequence<sizeof...(A)>()); }

private:
  template <size_t... I> auto run(std::index_sequence<I...>) {
    return this_ns::invoke(std::move(func_), std::move(std::get<I>(args_))...);
  }

  std::decay_t<F> func_;
  std::tuple<std::decay_t<A>...> args_;
};

template <typename F, typename... A> task<F, A...> make_task(F &&f, A &&...a) {
  return {std::forward<F>(f), std::forward<A>(a)...};
}

} // namespace detail

/**
 * @ingroup future_hdr
 * @brief Executor aware analog of the `std::async`.
 *
 * Runs the function `func` with arguments `a` asynchronously using executor
 * `exec` and returns a @ref future that will eventually hold the result of that
 * function call. The `func` and `a` parameters are decay-copied before sending
 * to executor.
 *
 * If `std::result_of_t<F(A...)>` is either `future<T>` or `shared_future<T>`
 * then @ref unwrap "unwrapped" `future<T>` or `shared_future<T>` is returned.
 *
 * The function participates in overload resolution only if
 * `is_executor<E>::value` is `true`.
 */
#if defined(DOXYGEN)
template <typename E, typename F, typename... A>
future<std::result_of_t<F(A...)>> async(E &&exec, F &&func, A &&...a) {
#else
template <typename E, typename F, typename... A>
PC_NODISCARD auto async(E &&exec, F &&func, A &&...a) -> std::enable_if_t<
    is_executor<std::decay_t<E>>::value,
    detail::add_future_t<detail::invoke_result_t<F, A...>>> {
#endif
  using R = detail::invoke_result_t<F, A...>;
  packaged_task<R()> task{
      detail::make_task(std::forward<F>(func), std::forward<A>(a)...)};
  detail::add_future_t<R> f = task.get_future();
  post(exec, std::move(task));
  return f;
}

/**
 * @page unwrap Implicit unwrapping
 *
 * If some function returning type `R` is going to be executed asynchronously
 * (via `async`, `packaged_task` or as continuation for some `future` or
 * `shared_future` object) then `future<R>` object is created in order to
 * provide access to the result of the function invocation. If function itself
 * returns object of type `future<R>` or `shared_future<R>` then running it
 * asynchronously in a naive implementation will create future to future to
 * result. Such `future<future<R>>` type is quite inconvenient to use and
 * actually forbidden by the portable_concurrency `future` and `shared_future`
 * implementation (same is true for any combination of future templates
 * `future<shared_future<R>>`, `shared_future<future<R>>` and
 * `shared_future<shared_future<R>>`). `future<R>` is always created instead of
 * `future<future<R>>` and `shared_future<R>` instead of
 * `future<shared_future<R>>` by applying rules specified in "The C++ Extensions
 * for Concurrency" TS as implicit unwrapping.
 *
 * The future unwrapping works as if there are two function template overloads:
 *  @li `template<typename R> future<R> UNWRAP(future<future<R>>&& rhs)`
 *  @li `template<typename R> shared_future<R> UNWRAP(future<shared_future<R>>&&
 * rhs)`
 *
 * which constructs `future`/`shared_future` object from the shared state
 * referred to by `rhs`. The object becomes ready when one of the following
 * occurs:
 *  @li Both the `rhs` and `rhs.get()` are ready. The value or the exception
 * from `rhs.get()` is stored in the returned object shared state.
 *  @li `rhs` is ready but `rhs.get()` is invalid. The returned object stores an
 * exception of type `std::future_error`, with an error condition of
 * `std::future_errc::broken_promise`.
 *
 * @note Same logic is extended to be applicable for `promise<future<T>>` and
 * `promise<shared_future<T>>`. `promise::get` member function works as
 * `UNWRAP(promise_to_future.NAIVE_GET())`.
 */

} // namespace cxx14_v1
} // namespace portable_concurrency
