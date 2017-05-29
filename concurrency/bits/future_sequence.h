#pragma once

#include <tuple>
#include <utility>
#include <vector>

#include "concurrency_type_traits.h"
#include "future.h"
#include "shared_future.h"
#include "shared_state.h"

namespace experimental {
inline namespace concurrency_v1 {
namespace detail {

template<typename T>
future_state<T>* state_of(future<T>& f) {
  return f.state_.get();
}

template<typename T>
future_state<T>* state_of(shared_future<T>& f) {
  return f.state_.get();
}

inline void swallow(...) {}

template<typename Sequence>
struct sequence_traits;

template<typename Future>
struct sequence_traits<std::vector<Future>> {
  static std::size_t index_of_ready(const std::vector<Future>& seq) {
    std::size_t res = 0;
    for (const auto& f: seq) {
      if (f.is_ready())
        return res;
      ++res;
    }
    return static_cast<std::size_t>(-1);
  }

  static std::size_t size(const std::vector<Future>& seq) {
    return seq.size();
  }

  static void attach_continuation(
    std::vector<Future>& seq,
    const std::shared_ptr<continuation>& cnt
  ) {
    // TODO: use set_continuation for future and add_continuation for shared_future
    for (auto& f: seq)
      ::experimental::concurrency_v1::detail::state_of(f)->add_continuation(cnt);
  }
};

template<typename... Futures>
struct sequence_traits<std::tuple<Futures...>> {
  static std::size_t index_of_ready(const std::tuple<Futures...>& seq) {
    return index_of_ready(seq, std::make_index_sequence<sizeof...(Futures)>{});
  }

  static std::size_t size(const std::tuple<Futures...>& seq) {
    return sizeof...(Futures);
  }

  static void attach_continuation(
    std::tuple<Futures...>& seq,
    const std::shared_ptr<continuation>& cnt
  ) {
    attach_continuation(seq, cnt, std::make_index_sequence<sizeof...(Futures)>{});
  }

private:
  template<size_t... I>
  static std::size_t index_of_ready(const std::tuple<Futures...>& seq, std::index_sequence<I...>) {
    bool statuses[] = {std::get<I>(seq).is_ready()...};
    std::size_t res = 0;
    for (bool ready: statuses) {
      if (ready)
        return res;
      ++res;
    }
    return static_cast<std::size_t>(-1);
  }

  template<size_t... I>
  static void attach_continuation(
    std::tuple<Futures...>& seq,
    const std::shared_ptr<continuation>& cnt,
    std::index_sequence<I...>
  ) {
    // TODO: use set_continuation for future and add_continuation for shared_future
    swallow((
      ::experimental::concurrency_v1::detail::state_of(std::get<I>(seq))->add_continuation(cnt), 0
    )...);
  }
};

} // namespace detail
} // inline namespace concurrency_v1
} // namespace experimental
