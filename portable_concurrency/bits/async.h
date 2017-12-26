#pragma once

#include "concurrency_type_traits.h"
#include "execution.h"
#include "make_future.h"
#include "shared_state.h"

namespace portable_concurrency {
inline namespace cxx14_v1 {

template<typename E, typename F, typename... A>
auto async(E&& exec, F&& func, A&&... a) -> std::enable_if_t<
  is_executor<std::decay_t<E>>::value,
  detail::add_future_t<std::result_of_t<F(A...)>>
> {
  // TODO: provide better implementation
  return make_ready_future().next(
    std::forward<E>(exec),
    std::bind(std::forward<F>(func), std::forward<A>(a)...)
  );
}

} // inline namespace cxx14_v1
} // namespace portable_concurrency
