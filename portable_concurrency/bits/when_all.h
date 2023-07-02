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

template <typename Sequence>
class when_all_state final : public future_state<Sequence> {
public:
  when_all_state(Sequence &&futures)
      : futures_(std::move(futures)),
        operations_remains_(sequence_traits<Sequence>::size(futures_) + 1) {}

  static std::shared_ptr<future_state<Sequence>> make(Sequence &&futures) {
    auto state = std::make_shared<when_all_state<Sequence>>(std::move(futures));
    sequence_traits<Sequence>::for_each(state->futures_, [state](auto &f) {
      state_of(f)->continuations().push([state] { state->notify(); });
    });
    state->notify();
    return state;
  }

  void notify() {
    if (--operations_remains_ != 0)
      return;
    continuations_.execute();
  }

  Sequence &value_ref() override {
    assert(continuations_.executed());
    return futures_;
  }

  std::exception_ptr exception() override {
    assert(continuations_.executed());
    return nullptr;
  }

  continuations_stack &continuations() final { return continuations_; }

private:
  Sequence futures_;
  std::atomic<size_t> operations_remains_;
  continuations_stack continuations_;
};

} // namespace detail

PC_NODISCARD future<std::tuple<>> when_all();

/**
 * @ingroup future_hdr
 *
 * Create a future object that becomes ready when all of the input futures and
 * shared_futures become ready. The behavior is undefined if any input future or
 * shared_future is invalid. Ready `future<std::tuple<>>` is immediately
 * returned if input futures are empty.
 *
 * This function template participates in overload resolution only if all of the
 * arguments are either `future<T>` or `shared_future<T>`.
 */
#ifdef DOXYGEN
template <typename... Futures>
future<std::tuple<Futures...>> when_all(Futures &&...);
#else
template <typename... Futures>
PC_NODISCARD auto when_all(Futures &&...futures)
    -> std::enable_if_t<detail::are_futures<std::decay_t<Futures>...>::value,
                        future<std::tuple<std::decay_t<Futures>...>>> {
  using Sequence = std::tuple<std::decay_t<Futures>...>;
  return {detail::when_all_state<Sequence>::make(
      Sequence{std::forward<Futures>(futures)...})};
}
#endif

/**
 * @ingroup future_hdr
 *
 * Create a future object that becomes ready when all of the input futures and
 * shared_futures become ready. The behavior is undefined if any input future or
 * shared_future is invalid. Ready `future<std::tuple<>>` is immediately
 * returned if input futures is empty range (`first == last`). Every input
 * `future<T>` object is moved into corresponding object of the returned future
 * shared state, and every `shared_future<T>` object is copied. The order of the
 * objects in the returned `future` object matches the order of arguments.
 *
 * This function template participates in overload resolution only if value type
 * of the InputIt is either `future<T>` or `shared_future<T>`.
 */
#ifdef DOXYGEN
template <typename InputIt>
future<std::vector<typename std::iterator_traits<InputIt>::value_type>>
when_all(InputIt first, InputIt last);
#else
template <typename InputIt>
PC_NODISCARD auto when_all(InputIt first, InputIt last) -> std::enable_if_t<
    detail::is_unique_future<
        typename std::iterator_traits<InputIt>::value_type>::value,
    future<std::vector<typename std::iterator_traits<InputIt>::value_type>>> {
  using Sequence =
      std::vector<typename std::iterator_traits<InputIt>::value_type>;
  if (first == last)
    return make_ready_future(Sequence{});
  return {detail::when_all_state<Sequence>::make(
      Sequence{std::make_move_iterator(first), std::make_move_iterator(last)})};
}

template <typename InputIt>
PC_NODISCARD auto when_all(InputIt first, InputIt last) -> std::enable_if_t<
    detail::is_shared_future<
        typename std::iterator_traits<InputIt>::value_type>::value,
    future<std::vector<typename std::iterator_traits<InputIt>::value_type>>> {
  using Sequence =
      std::vector<typename std::iterator_traits<InputIt>::value_type>;
  if (first == last)
    return make_ready_future(Sequence{});
  return {detail::when_all_state<Sequence>::make(Sequence{first, last})};
}
#endif

/**
 * @ingroup future_hdr
 *
 * Create a future object that becomes ready when all of the input futures and
 * shared_futures become ready. The behavior is undefined if any input future or
 * shared_future is invalid. Effectively equivalent to
 * `when_all(futures.begin(), futures.end())` but this overload reuses vector
 * passed as argument instead of making new one saving one extra allocation, and
 * supports vectors with user-provided allocators.
 *
 * This function template participates in overload resolution only if `Future`
 * is either `future<T>` or `shared_future<T>`.
 */
#ifdef DOXYGEN
template <typename Future, typename Alloc>
future<std::vector<Future, Alloc>> when_all(std::vector<Future, Alloc> futures);
#else
template <typename Future, typename Alloc>
PC_NODISCARD auto when_all(std::vector<Future, Alloc> futures)
    -> std::enable_if_t<detail::is_future<Future>::value,
                        future<std::vector<Future, Alloc>>> {
  using Sequence = std::vector<Future, Alloc>;
  return {detail::when_all_state<Sequence>::make(std::move(futures))};
}
#endif

} // namespace cxx14_v1
} // namespace portable_concurrency
