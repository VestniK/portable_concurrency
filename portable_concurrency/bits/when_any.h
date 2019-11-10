#pragma once

#include <tuple>
#include <type_traits>
#include <vector>

#include "fwd.h"

#include "concurrency_type_traits.h"
#include "future.h"
#include "future_sequence.h"
#include "make_future.h"
#include "shared_future.h"
#include "shared_state.h"

namespace portable_concurrency {
inline namespace cxx14_v1 {

template <typename Sequence>
struct when_any_result {
  std::size_t index;
  Sequence futures;
};

namespace detail {

template <typename Sequence>
class when_any_state final : public future_state<when_any_result<Sequence>> {
public:
  when_any_state(Sequence&& futures) : result_{static_cast<std::size_t>(-1), std::move(futures)} {}

  // threadsafe
  void notify(std::size_t pos) {
    if (ready_flag_.test_and_set())
      return;
    result_.index = pos;
    continuations_.execute();
  }

  static std::shared_ptr<future_state<when_any_result<Sequence>>> make(Sequence&& seq) {
    auto state = std::make_shared<when_any_state<Sequence>>(std::move(seq));
    std::size_t idx = 0;
    sequence_traits<Sequence>::for_each(state->result_.futures, [state, &idx](auto& f) mutable {
      state_of(f)->continuations().push([state, pos = idx++] { state->notify(pos); });
    });
    if (idx == 0)
      state->continuations_.execute();
    return state;
  }

  when_any_result<Sequence>& value_ref() override {
    assert(continuations_.executed());
    return result_;
  }

  std::exception_ptr exception() override {
    assert(continuations_.executed());
    return nullptr;
  }

  continuations_stack& continuations() final { return continuations_; }

private:
  when_any_result<Sequence> result_;
  std::atomic_flag ready_flag_ = ATOMIC_FLAG_INIT;
  continuations_stack continuations_;
};

} // namespace detail

future<when_any_result<std::tuple<>>> when_any();

template <typename... Futures>
auto when_any(Futures&&... futures) -> std::enable_if_t<detail::are_futures<std::decay_t<Futures>...>::value,
    future<when_any_result<std::tuple<std::decay_t<Futures>...>>>> {
  using Sequence = std::tuple<std::decay_t<Futures>...>;
  return future<when_any_result<Sequence>>{
      detail::when_any_state<Sequence>::make(Sequence{std::forward<Futures>(futures)...})};
}

template <typename InputIt>
auto when_any(InputIt first, InputIt last)
    -> std::enable_if_t<detail::is_unique_future<typename std::iterator_traits<InputIt>::value_type>::value,
        future<when_any_result<std::vector<typename std::iterator_traits<InputIt>::value_type>>>> {
  using Sequence = std::vector<typename std::iterator_traits<InputIt>::value_type>;
  return future<when_any_result<Sequence>>{
      detail::when_any_state<Sequence>::make(Sequence{std::make_move_iterator(first), std::make_move_iterator(last)})};
}

template <typename InputIt>
auto when_any(InputIt first, InputIt last)
    -> std::enable_if_t<detail::is_shared_future<typename std::iterator_traits<InputIt>::value_type>::value,
        future<when_any_result<std::vector<typename std::iterator_traits<InputIt>::value_type>>>> {
  using Sequence = std::vector<typename std::iterator_traits<InputIt>::value_type>;
  return future<when_any_result<Sequence>>{detail::when_any_state<Sequence>::make(Sequence{first, last})};
}

template <typename Future, typename Alloc>
auto when_any(std::vector<Future, Alloc> futures)
    -> std::enable_if_t<detail::is_future<Future>::value, future<when_any_result<std::vector<Future, Alloc>>>> {
  using Sequence = std::vector<Future, Alloc>;
  return {detail::when_any_state<Sequence>::make(std::move(futures))};
}

} // namespace cxx14_v1
} // namespace portable_concurrency
