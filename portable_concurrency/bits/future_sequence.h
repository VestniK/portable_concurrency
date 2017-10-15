#pragma once

#include <tuple>
#include <utility>
#include <vector>

#include "concurrency_type_traits.h"
#include "future.h"
#include "shared_future.h"
#include "shared_state.h"

namespace portable_concurrency {
inline namespace cxx14_v1 {
namespace detail {

struct swallow {
  swallow(...) {}
};

template<typename Sequence>
struct sequence_traits;

template<typename Future>
struct sequence_traits<std::vector<Future>> {
  static std::size_t size(const std::vector<Future>& seq) {
    return seq.size();
  }

  template<typename F>
  static void for_each(std::vector<Future>& seq, F&& func) {
    for (auto& f: seq)
      std::forward<F>(func)(f);
  }
};

template<typename... Futures>
struct sequence_traits<std::tuple<Futures...>> {
  static constexpr std::size_t size(const std::tuple<Futures...>&) {
    return sizeof...(Futures);
  }

  template<typename F>
  static void for_each(
    std::tuple<Futures...>& seq,
    F&& func
  ) {
    for_each(seq, std::forward<F>(func), std::make_index_sequence<sizeof...(Futures)>{});
  }

private:
  template<typename F, size_t... I>
  static void for_each(
    std::tuple<Futures...>& seq,
    F&& func,
    std::index_sequence<I...>
  ) {
    swallow{(
      (void)(std::forward<F>(func)(std::get<I>(seq))), 0
    )...};
  }
};

} // namespace detail
} // inline namespace cxx14_v1
} // namespace portable_concurrency
