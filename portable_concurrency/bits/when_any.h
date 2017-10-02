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

template<typename Sequence>
struct when_any_result {
  std::size_t index;
  Sequence futures;
};

namespace detail {

template<typename Sequence>
class when_any_state final: public future_state<when_any_result<Sequence>>
{
public:
  when_any_state(Sequence&& futures):
    result_{static_cast<std::size_t>(-1), std::move(futures)}
  {}

  // threadsafe
  void notify(std::size_t pos) {
    if (ready_flag_.test_and_set())
      return;
    result_.index = pos;
    for (auto& cnt: continuations_.consume())
      cnt();
  }

  static std::shared_ptr<future_state<when_any_result<Sequence>>> make(Sequence&& seq) {
#if defined(_LIBCPP_VERSION) && _LIBCPP_VERSION < 3900
    // looks like std::make_shared is affected by https://bugs.llvm.org/show_bug.cgi?id=22806
    auto state = std::shared_ptr<when_any_state<Sequence>>{
      new when_any_state<Sequence>{std::move(seq)}
    };
#else
    auto state = std::make_shared<when_any_state<Sequence>>(std::move(seq));
#endif
    sequence_traits<Sequence>::for_each(
      state->result_.futures,
      [state, idx = std::size_t(0)](auto& f) mutable {
        state_of(f)->add_continuation([state, pos = idx++] {state->notify(pos);});
      }
    );
    return state;
  }

  bool is_ready() const override {return continuations_.is_consumed();}
  void add_continuation(unique_function<void()> cnt) {
    if (!continuations_.push(cnt))
      cnt();
  }
  wait_continuation& get_waiter() override {return continuations_.get_waiter();}

  when_any_result<Sequence>& value_ref() override {
    assert(is_ready());
    return result_;
  }

private:
  when_any_result<Sequence> result_;
  std::atomic_flag ready_flag_ = ATOMIC_FLAG_INIT;
  continuations_stack continuations_;
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

} // inline namespace cxx14_v1
} // namespace portable_concurrency
