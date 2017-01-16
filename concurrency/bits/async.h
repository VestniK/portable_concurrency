#pragma once

#include <future>

#include "future.h"

namespace experimental {
inline namespace concurrency_v1 {

template<typename F, typename... A>
future<std::result_of_t<std::decay_t<F>(std::decay_t<A>...)>> async(std::launch launch, F&& f, A&&... a) {
  if (launch == std::launch::async) {
    throw std::system_error(
      std::make_error_code(std::errc::resource_unavailable_try_again),
      "Futures with blocking destrctor are not supported"
    );
  }
  // TODO: implement defered
  return {};
}

template<typename F, typename... A>
auto async(F&& f, A&&... a) {
  return async(std::launch::deferred, std::forward<F>(f), std::forward<A>(a)...);
}

} // inline namespace concurrency_v1
} // namespace experimental
