#pragma once

#include "fwd.h"

#include "invoke.h"
#include "shared_state.h"

namespace portable_concurrency {
inline namespace cxx14_v1 {
namespace detail {

template <typename R, typename F, typename... A>
void set_state_value(std::shared_ptr<shared_state<R>> &state, F &&f, A &&...a) {
  assert(state);
  bool executed = false;
  try {
    auto &&res = ::portable_concurrency::cxx14_v1::detail::invoke(
        std::forward<F>(f), std::forward<A>(a)...);
    executed = true;
    shared_state<R>::unwrap(state, std::move(res));
  } catch (...) {
    if (executed)
      throw;
    state->set_exception(std::current_exception());
  }
}

template <typename R, typename F, typename... A>
std::enable_if_t<!is_future<invoke_result_t<F, A...>>::value>
set_state_value(std::shared_ptr<shared_state<R &>> &state, F &&f, A &&...a) {
  assert(state);
  bool executed = false;
  try {
    R &res = ::portable_concurrency::cxx14_v1::detail::invoke(
        std::forward<F>(f), std::forward<A>(a)...);
    executed = true;
    shared_state<R &>::unwrap(state, res);
  } catch (...) {
    if (executed)
      throw;
    state->set_exception(std::current_exception());
  }
}

template <typename R, typename F, typename... A>
std::enable_if_t<is_future<invoke_result_t<F, A...>>::value>
set_state_value(std::shared_ptr<shared_state<R &>> &state, F &&f,
                A &&...a) try {
  shared_state<R &>::unwrap(state,
                            ::portable_concurrency::cxx14_v1::detail::invoke(
                                std::forward<F>(f), std::forward<A>(a)...));
} catch (...) {
  state->set_exception(std::current_exception());
}

template <typename F, typename... A>
std::enable_if_t<std::is_void<invoke_result_t<F, A...>>::value>
set_state_value(std::shared_ptr<shared_state<void>> &state, F &&f, A &&...a) {
  assert(state);
  bool executed = false;
  try {
    ::portable_concurrency::cxx14_v1::detail::invoke(std::forward<F>(f),
                                                     std::forward<A>(a)...);
    executed = true;
    state->emplace();
  } catch (...) {
    if (executed)
      throw;
    state->set_exception(std::current_exception());
  }
}

template <typename F, typename... A>
std::enable_if_t<!std::is_void<invoke_result_t<F, A...>>::value>
set_state_value(std::shared_ptr<shared_state<void>> &state, F &&f, A &&...a) {
  bool executed = false;
  try {
    shared_state<void>::unwrap(
        state, state_of(::portable_concurrency::cxx14_v1::detail::invoke(
                   std::forward<F>(f), std::forward<A>(a)...)));
  } catch (...) {
    if (executed)
      throw;
    state->set_exception(std::current_exception());
  }
}

} // namespace detail
} // namespace cxx14_v1
} // namespace portable_concurrency
