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
  using continuation = typename future_state<when_any_state<Sequence>>::continuation;
public:
  when_any_state(Sequence&& futures):
    result_{static_cast<std::size_t>(-1), std::move(futures)}
  {}

  // threadsafe
  void notify(std::size_t pos) {
    if (ready_flag_.test_and_set())
      return;
    result_.index = pos;
    continuations_.execute();
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
        state_of(f)->push_continuation([state, pos = idx++] {state->notify(pos);});
      }
    );
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

  void push_continuation(continuation&& cnt) final {
    continuations_.push(std::move(cnt));
  }
  void execute_continuations() final {
    continuations_.execute();
  }
  bool continuations_executed() const final {
    return continuations_.executed();
  }
  void wait() final {
    continuations_.wait();
  }
  bool wait_for(std::chrono::nanoseconds timeout) final {
    return continuations_.wait_for(timeout);
  }

private:
  when_any_result<Sequence> result_;
  std::atomic_flag ready_flag_ = ATOMIC_FLAG_INIT;
  continuations_stack<std::allocator<continuation>> continuations_;
};

} // namespace detail

future<when_any_result<std::tuple<>>> when_any();

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
