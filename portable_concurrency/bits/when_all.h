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

namespace portable_concurrency {
inline namespace cxx14_v1 {

namespace detail {

template<typename Sequence>
class when_all_state final: public future_state<Sequence>
{
  using continuation = typename future_state<Sequence>::continuation;
public:
  when_all_state(Sequence&& futures):
    futures_(std::move(futures)),
    operations_remains_(sequence_traits<Sequence>::size(futures_) + 1)
  {}

  static std::shared_ptr<future_state<Sequence>> make(Sequence&& futures) {
#if defined(_LIBCPP_VERSION) && _LIBCPP_VERSION < 3900
    // looks like std::make_shared is affected by https://bugs.llvm.org/show_bug.cgi?id=22806
    std::shared_ptr<when_all_state<Sequence>> state{
      new when_all_state<Sequence>(std::move(futures))
    };
#else
    auto state = std::make_shared<when_all_state<Sequence>>(std::move(futures));
#endif
    sequence_traits<Sequence>::for_each(state->futures_, [state](auto& f) {
      state_of(f)->push_continuation([state]{state->notify();});
    });
    state->notify();
    return state;
  }

  void notify() {
    if (--operations_remains_ != 0)
      return;
    continuations_.execute();
  }

  Sequence& value_ref() override {
    assert(continuations_.executed());
    return futures_;
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
  Sequence futures_;
  std::atomic<size_t> operations_remains_;
  continuations_stack<std::allocator<continuation>> continuations_;
};

} // namespace detail

future<std::tuple<>> when_all();

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

} // inline namespace cxx14_v1
} // namespace portable_concurrency
