#pragma once

#include "fwd.h"

#include "shared_state.h"

namespace concurrency {
namespace detail {

template<typename T>
std::decay_t<T> decay_copy(T&& t) {return std::forward<T>(t);}

template<typename T>
future<T> make_future(std::shared_ptr<shared_state<T>>&& state) {
  return future<T>{std::move(state)};
}

} // namespace detail
} // namespace concurrency
