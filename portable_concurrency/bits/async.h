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

template <typename F, typename... A>
class task {
public:
  task(F&& f, A&&... a) : func_(std::forward<F>(f)), args_(std::forward<A>(a)...) {}

  auto operator()() { return run(std::make_index_sequence<sizeof...(A)>()); }

private:
  template <size_t... I>
  auto run(std::index_sequence<I...>) {
    return this_ns::invoke(std::move(func_), std::move(std::get<I>(args_))...);
  }

  std::decay_t<F> func_;
  std::tuple<std::decay_t<A>...> args_;
};

template <typename F, typename... A>
task<F, A...> make_task(F&& f, A&&... a) {
  return {std::forward<F>(f), std::forward<A>(a)...};
}

} // namespace detail

/**
 * @ingroup future_hdr
 * @brief Executor aware analog of the `std::async`.
 *
 * Runs the function `func` with arguments `a` asynchronyously using executor `exec` and returns a @ref future that will
 * eventually hold the result of that function call. The `func` and `a` parameters are decay-copied before sending
 * to executor.
 *
 * If `std::result_of_t<F(A...)>` is either `future<T>` or `shared_future<T>` then @ref unwrap "unwrapped" `future<T>`
 * or `shared_future<T>` is returned.
 *
 * The function participates in overload resolution only if `is_executor<E>::value` is `true`.
 */
#if defined(DOXYGEN)
template <typename E, typename F, typename... A>
future<std::result_of_t<F(A...)>> async(E&& exec, F&& func, A&&... a) {
#else
template <typename E, typename F, typename... A>
PC_NODISCARD auto async(E&& exec, F&& func, A&&... a)
    -> std::enable_if_t<is_executor<std::decay_t<E>>::value, detail::add_future_t<std::result_of_t<F(A...)>>> {
#endif
  using R = std::result_of_t<F(A...)>;
  packaged_task<R()> task{detail::make_task(std::forward<F>(func), std::forward<A>(a)...)};
  detail::add_future_t<R> f = task.get_future();
  post(exec, std::move(task));
  return f;
}

/**
 * @page unwrap Implicit unwrap
 *
 *
 */

} // namespace cxx14_v1
} // namespace portable_concurrency
