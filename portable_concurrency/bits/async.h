#pragma once

#include <functional>

#include "concurrency_type_traits.h"
#include "execution.h"
#include "make_future.h"
#include "shared_state.h"

#include <portable_concurrency/bits/config.h>

namespace portable_concurrency {
inline namespace cxx14_v1 {

/**
 * @ingroup future_hdr
 * @brief Executor aware analog of the `std::async`.
 *
 * Runs the function `func` with arguments `a` asynchronyously using executor `exec` and returns a @ref future that will
 * eventually hold the result of that function call. The `func` and `a` parameters are decay-copied before sending
 * to executor.
 *
 * The function participates in overload resolution only if `is_executor<E>::value` is `true`
 */
template <typename E, typename F, typename... A>
PC_NODISCARD
auto async(E&& exec, F&& func, A&&... a)
    -> std::enable_if_t<is_executor<std::decay_t<E>>::value, detail::add_future_t<std::result_of_t<F(A...)>>> {
  // TODO: provide better implementation
  return make_ready_future().next(std::forward<E>(exec), std::bind(std::forward<F>(func), std::forward<A>(a)...));
}

} // namespace cxx14_v1
} // namespace portable_concurrency
