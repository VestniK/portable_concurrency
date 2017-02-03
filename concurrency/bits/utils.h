#pragma once

#include "fwd.h"

#include "invoke.h"
#include "shared_state.h"

namespace experimental {
inline namespace concurrency_v1 {
namespace detail {

template<typename T>
std::decay_t<T> decay_copy(T&& t) {return std::forward<T>(t);}

template<typename T>
future<T> make_future(std::shared_ptr<shared_state<T>>&& state) {
  return future<T>{std::move(state)};
}

template<typename R, typename F, typename... A>
void set_state_value(shared_state<R>& state, F&& f, A&&... a) noexcept try {
  state.emplace(
    ::experimental::concurrency_v1::detail::invoke(std::forward<F>(f), std::forward<A>(a)...)
  );
} catch (...) {
  state.set_exception(std::current_exception());
}

template<typename F, typename... A>
void set_state_value(shared_state<void>& state, F&& f, A&&... a) noexcept try {
  ::experimental::concurrency_v1::detail::invoke(std::forward<F>(f), std::forward<A>(a)...);
  state.emplace();
} catch (...) {
  state.set_exception(std::current_exception());
}

} // namespace detail
} // inline namespace concurrency_v1
} // namespace experimental
