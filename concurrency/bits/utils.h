#pragma once

#include "fwd.h"

#include "invoke.h"
#include "shared_state.h"

namespace experimental {
inline namespace concurrency_v1 {
namespace detail {

template<typename T>
std::decay_t<T> decay_copy(T&& t) {return std::forward<T>(t);}

template<typename R, typename F, typename... A>
void set_state_value(shared_state<R>& state, F&& f, A&&... a) {
  bool executed = false;
  try {
    auto&& res = ::experimental::concurrency_v1::detail::invoke(std::forward<F>(f), std::forward<A>(a)...);
    executed = true;
    state.emplace(std::move(res));
  } catch (...) {
    if (executed)
      throw;
    state.set_exception(std::current_exception());
  }
}

template<typename R, typename F, typename... A>
void set_state_value(shared_state<R&>& state, F&& f, A&&... a) {
  bool executed = false;
  try {
    R& res = ::experimental::concurrency_v1::detail::invoke(std::forward<F>(f), std::forward<A>(a)...);
    executed = true;
    state.emplace(res);
  } catch (...) {
    if (executed)
      throw;
    state.set_exception(std::current_exception());
  }
}

template<typename F, typename... A>
void set_state_value(shared_state<void>& state, F&& f, A&&... a) {
  bool executed = false;
  try {
    ::experimental::concurrency_v1::detail::invoke(std::forward<F>(f), std::forward<A>(a)...);
    executed = true;
    state.emplace();
  } catch (...) {
    if (executed)
      throw;
    state.set_exception(std::current_exception());
  }
}

} // namespace detail
} // inline namespace concurrency_v1
} // namespace experimental
