#pragma once

#include <atomic>
#include <type_traits>
#include <utility>

#include "fwd.h"

#include "concurrency_type_traits.h"
#include "future.h"
#include "future_sequence.h"
#include "make_future.h"
#include "shared_future.h"
#include "shared_state.h"

namespace experimental {
inline namespace concurrency_v1 {

namespace detail {

template<typename Sequence>
class when_all_state:
  public shared_state<Sequence>,
  public continuation
{
public:
  when_all_state(Sequence&& futures): futures_(std::move(futures)) {}

  static std::shared_ptr<shared_state<Sequence>> make(Sequence&& futures) {
#if defined(_LIBCPP_VERSION) && _LIBCPP_VERSION < 3900
    // looks like std::make_shared is affected by https://bugs.llvm.org/show_bug.cgi?id=22806
    std::shared_ptr<when_all_state<Sequence>> state{
      new when_all_state<Sequence>(std::move(futures))
    };
#else
    auto state = std::make_shared<when_all_state<Sequence>>(std::move(futures));
#endif
    sequence_traits<Sequence>::attach_continuation(state->futures_, state);
    return state;
  }

  void invoke() override {
    if (++ready_count_ < sequence_traits<Sequence>::size(futures_))
      return;
    this->emplace(std::move(futures_));
  }

private:
  Sequence futures_;
  std::atomic<size_t> ready_count_{0};
};

} // namespace detail

inline
future<std::tuple<>> when_all() {
  return make_ready_future(std::tuple<>{});
}

template<typename... Futures>
auto when_all(Futures&&... futures) ->
  std::enable_if_t<
    detail::are_futures<std::decay_t<Futures>...>::value,
    future<std::tuple<std::decay_t<Futures>...>>
  >
{
  using Sequence = std::tuple<std::decay_t<Futures>...>;
  return {detail::when_all_state<Sequence>::make(Sequence{std::forward<Futures>(futures)...})};
}

template<typename InputIt>
auto when_all(InputIt first, InputIt last) ->
  std::enable_if_t<
    detail::is_unique_future<typename std::iterator_traits<InputIt>::value_type>::value,
    future<std::vector<typename std::iterator_traits<InputIt>::value_type>>
  >
{
  using Sequence = std::vector<typename std::iterator_traits<InputIt>::value_type>;
  if (first == last)
    return make_ready_future(Sequence{});
  return {detail::when_all_state<Sequence>::make(
    Sequence{std::make_move_iterator(first), std::make_move_iterator(last)}
  )};
}

template<typename InputIt>
auto when_all(InputIt first, InputIt last) ->
  std::enable_if_t<
    detail::is_shared_future<typename std::iterator_traits<InputIt>::value_type>::value,
    future<std::vector<typename std::iterator_traits<InputIt>::value_type>>
  >
{
  using Sequence = std::vector<typename std::iterator_traits<InputIt>::value_type>;
  if (first == last)
    return make_ready_future(Sequence{});
  return {detail::when_all_state<Sequence>::make(Sequence{first, last})};
}

} // inline namespace concurrency_v1
} // namespace experimental
