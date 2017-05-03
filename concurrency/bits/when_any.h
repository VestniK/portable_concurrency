#pragma once

#include <tuple>
#include <type_traits>
#include <utility>

#include "fwd.h"

#include "concurrency_type_traits.h"
#include "future.h"
#include "make_future.h"
#include "shared_future.h"
#include "shared_state.h"

namespace experimental {
inline namespace concurrency_v1 {

template<typename Sequence>
struct when_any_result {
  std::size_t index;
  Sequence futures;
};

namespace detail {

template<typename T>
shared_state<T>* state_of(future<T>& f) {
  return f.state_.get();
}

template<typename T>
shared_state<T>* state_of(shared_future<T>& f) {
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
    swallow((
      ::experimental::concurrency_v1::detail::state_of(std::get<I>(seq))->add_continuation(cnt), 0
    )...);
  }
};

template<typename Sequence>
class when_any_state:
  public shared_state<when_any_result<Sequence>>,
  public continuation
{
public:
  when_any_state(Sequence&& futures):
    futures_(std::move(futures)),
    barrier_(static_cast<int>(sequence_traits<Sequence>::size(futures_)) + 1)
  {}

  // threadsafe
  void invoke() override {
    if (barrier_.fetch_sub(1) != 1)
      return;
    set();
  }

  static std::shared_ptr<shared_state<when_any_result<Sequence>>> make(Sequence&& seq) {
    const int seq_sz = static_cast<int>(sequence_traits<Sequence>::size(seq));
#if defined(_LIBCPP_VERSION) && _LIBCPP_VERSION < 3900
    // looks like std::make_shared is affected by https://bugs.llvm.org/show_bug.cgi?id=22806
    auto state = std::shared_ptr<when_any_state<Sequence>>{
      new when_any_state<Sequence>{std::move(seq)}
    };
#else
    auto state = std::make_shared<when_any_state<Sequence>>(std::move(seq));
#endif
    sequence_traits<Sequence>::attach_continuation(state->futures_, state);
    if (state->barrier_.fetch_sub(seq_sz) <= seq_sz)
      state->set();
    return state;
  }

private:
  void set() {
    const auto ready_idx = sequence_traits<Sequence>::index_of_ready(futures_);
    this->emplace(when_any_result<Sequence>{
      ready_idx,
      std::move(futures_)
    });
  }

private:
  Sequence futures_;
  std::atomic<int> barrier_;
};

} // namespace detail

inline
future<when_any_result<std::tuple<>>> when_any() {
  return make_ready_future(when_any_result<std::tuple<>>{
    static_cast<std::size_t>(-1),
    std::tuple<>{}
  });
}

template<typename... Futures>
auto when_any(Futures&&... futures) ->
  std::enable_if_t<
    detail::are_futures<std::decay_t<Futures>...>::value,
    future<when_any_result<std::tuple<std::decay_t<Futures>...>>>
  >
{
  using Sequence = std::tuple<std::decay_t<Futures>...>;
  return future<when_any_result<Sequence>>{detail::when_any_state<Sequence>::make(
    Sequence{std::forward<Futures>(futures)...}
  )};
}

template<typename InputIt>
auto when_any(InputIt first, InputIt last) ->
  std::enable_if_t<
    detail::is_unique_future<typename std::iterator_traits<InputIt>::value_type>::value,
    future<when_any_result<std::vector<typename std::iterator_traits<InputIt>::value_type>>>
  >
{
  using Sequence = std::vector<typename std::iterator_traits<InputIt>::value_type>;
  if (first == last)
    return make_ready_future(when_any_result<Sequence>{static_cast<std::size_t>(-1), {}});
  return future<when_any_result<Sequence>>{detail::when_any_state<Sequence>::make(
    Sequence{std::make_move_iterator(first), std::make_move_iterator(last)}
  )};
}

template<typename InputIt>
auto when_any(InputIt first, InputIt last) ->
  std::enable_if_t<
    detail::is_shared_future<typename std::iterator_traits<InputIt>::value_type>::value,
    future<when_any_result<std::vector<typename std::iterator_traits<InputIt>::value_type>>>
  >
{
  using Sequence = std::vector<typename std::iterator_traits<InputIt>::value_type>;
  if (first == last)
    return make_ready_future(when_any_result<Sequence>{static_cast<std::size_t>(-1), {}});
  return future<when_any_result<Sequence>>{detail::when_any_state<Sequence>::make(
    Sequence{first, last}
  )};
}

} // inline namespace concurrency_v1
} // namespace experimental
