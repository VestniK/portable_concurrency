#pragma once

#include "fwd.h"

#include "invoke.h"
#include "shared_state.h"

namespace portable_concurrency {
inline namespace cxx14_v1 {
namespace detail {

template<typename R, typename F, typename... A>
void set_state_value(basic_shared_state<R>& state, F&& f, A&&... a) {
  bool executed = false;
  try {
    auto&& res = ::portable_concurrency::cxx14_v1::detail::invoke(std::forward<F>(f), std::forward<A>(a)...);
    executed = true;
    state.emplace(std::move(res));
  } catch (...) {
    if (executed)
      throw;
    state.set_exception(std::current_exception());
  }
}

template<typename R, typename F, typename... A>
void set_state_value(basic_shared_state<R&>& state, F&& f, A&&... a) {
  bool executed = false;
  try {
    R& res = ::portable_concurrency::cxx14_v1::detail::invoke(std::forward<F>(f), std::forward<A>(a)...);
    executed = true;
    state.emplace(res);
  } catch (...) {
    if (executed)
      throw;
    state.set_exception(std::current_exception());
  }
}

template<typename F, typename... A>
void set_state_value(basic_shared_state<void>& state, F&& f, A&&... a) {
  bool executed = false;
  try {
    ::portable_concurrency::cxx14_v1::detail::invoke(std::forward<F>(f), std::forward<A>(a)...);
    executed = true;
    state.emplace();
  } catch (...) {
    if (executed)
      throw;
    state.set_exception(std::current_exception());
  }
}

} // namespace detail
} // inline namespace cxx14_v1
} // namespace portable_concurrency
