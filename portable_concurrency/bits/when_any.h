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

/*!
 * @ingroup future_hdr
 *
 * Structure holding sequence of futures passed as arguments to one of the
 * `when_any` function overloads and the index of a future in this sequence
 * which is ready.
 */
template <typename Sequence> struct when_any_result {
  //! Index of a ready future in the `futures` field.
  std::size_t index;
  //! Sequence of futures passed as an argument to `when_any` function. Input
  //! order is always preserved.
  Sequence futures;
};

namespace detail {

template <typename Sequence>
class when_any_state final : public future_state<when_any_result<Sequence>> {
public:
  when_any_state(Sequence &&futures)
      : result_{static_cast<std::size_t>(-1), std::move(futures)} {}

  // thread-safe
  void notify(std::size_t pos) {
    if (ready_flag_.test_and_set())
      return;
    result_.index = pos;
    continuations_.execute();
  }

  static std::shared_ptr<future_state<when_any_result<Sequence>>>
  make(Sequence &&seq) {
    auto state = std::make_shared<when_any_state<Sequence>>(std::move(seq));
    std::size_t idx = 0;
    sequence_traits<Sequence>::for_each(
        state->result_.futures, [state, &idx](auto &f) mutable {
          state_of(f)->continuations().push(
              [state, pos = idx++] { state->notify(pos); });
        });
    if (idx == 0)
      state->continuations_.execute();
    return state;
  }

  when_any_result<Sequence> &value_ref() override {
    assert(continuations_.executed());
    return result_;
  }

  std::exception_ptr exception() override {
    assert(continuations_.executed());
    return nullptr;
  }

  continuations_stack &continuations() final { return continuations_; }

private:
  when_any_result<Sequence> result_;
  std::atomic_flag ready_flag_ = ATOMIC_FLAG_INIT;
  continuations_stack continuations_;
};

} // namespace detail

PC_NODISCARD future<when_any_result<std::tuple<>>> when_any();

/**
 * @ingroup future_hdr
 *
 * Create a future object that becomes ready when at least one of the input
 * futures and shared_futures becomes ready. The behavior is undefined if any
 * input future or shared_future is invalid. Ready
 * `future<when_any_result<std::tuple<>>>` is immediately returned if input
 * futures are empty and the `index` field is `size_t(-1)`.
 *
 * The returned future is always `valid()`, and it becomes ready when at least
 * one of the input futures and shared_futures of the call is ready. The `index
 * member` of the `when_any_result` contains the position of the ready future or
 * shared_future in the `futures` member.
 *
 * This function template participates in overload resolution only if all of the
 * arguments are either `future<T>` or `shared_future<T>`.
 */
#ifdef DOXYGEN
template <typename... Futures>
future<when_any_result<std::tuple<Futures...>>> when_any(Futures &&...);
#else
template <typename... Futures>
PC_NODISCARD auto when_any(Futures &&...futures) -> std::enable_if_t<
    detail::are_futures<std::decay_t<Futures>...>::value,
    future<when_any_result<std::tuple<std::decay_t<Futures>...>>>> {
  using Sequence = std::tuple<std::decay_t<Futures>...>;
  return future<when_any_result<Sequence>>{
      detail::when_any_state<Sequence>::make(
          Sequence{std::forward<Futures>(futures)...})};
}
#endif

/**
 * @ingroup future_hdr
 *
 * Create a future object that becomes ready when at least one of the input
 * futures and shared_futures becomes ready. The behavior is undefined if any
 * input future or shared_future is invalid. Ready future immediatelly returned
 * if input futures is empty range (`first == last`) and the `index` field is
 * `size_t(-1)`. Every input `future<T>` object is moved into corresponding
 * object of the returned future shared state, and every `shared_future<T>`
 * object is copied. The order of the objects in the returned `future` object
 * matches the order of arguments.
 *
 * The returned future is always `valid()`, and it becomes ready when at least
 * one of the input futures and shared_futures of the call is ready. The `index
 * member` of the `when_any_result` contains the position of the ready future or
 * shared_future in the `futures` member.
 *
 * This function template participates in overload resolution only if value type
 * of the InputIt is either `future<T>` or `shared_future<T>`.
 */
#ifdef DOXYGEN
template <typename InputIt>
    future < when_any_result < std::vector <
    typename std::iterator_traits<InputIt>::value_type >>>>
    when_any(InputIt first, InputIt last);
#else
template <typename InputIt>
PC_NODISCARD auto when_any(InputIt first, InputIt last) -> std::enable_if_t<
    detail::is_unique_future<
        typename std::iterator_traits<InputIt>::value_type>::value,
    future<when_any_result<
        std::vector<typename std::iterator_traits<InputIt>::value_type>>>> {
  using Sequence =
      std::vector<typename std::iterator_traits<InputIt>::value_type>;
  return future<when_any_result<Sequence>>{
      detail::when_any_state<Sequence>::make(Sequence{
          std::make_move_iterator(first), std::make_move_iterator(last)})};
}
#endif

template <typename InputIt>
PC_NODISCARD auto when_any(InputIt first, InputIt last) -> std::enable_if_t<
    detail::is_shared_future<
        typename std::iterator_traits<InputIt>::value_type>::value,
    future<when_any_result<
        std::vector<typename std::iterator_traits<InputIt>::value_type>>>> {
  using Sequence =
      std::vector<typename std::iterator_traits<InputIt>::value_type>;
  return future<when_any_result<Sequence>>{
      detail::when_any_state<Sequence>::make(Sequence{first, last})};
}

#ifdef DOXYGEN
/**
 * @ingroup future_hdr
 *
 * Create a future object that becomes ready when at least one of the input
 * futures and shared_futures becomes ready. The behavior is undefined if any
 * input future or shared_future is invalid. Effectively equivalent to
 * `when_any(futures.begin(), futures.end())` but this overload reuses vector
 * passed as argument instead of making new one saving one extra allocation, and
 * supports vectors with user-provided allocators.
 *
 * This function template participates in overload resolution only if `Future`
 * is either `future<T>` or `shared_future<T>`.
 */
template <typename Future, typename Alloc>
future<when_any_result<std::vector<Future, Alloc>>>
when_any(std::vector<Future, Alloc> futures);
#else
template <typename Future, typename Alloc>
PC_NODISCARD auto when_any(std::vector<Future, Alloc> futures)
    -> std::enable_if_t<detail::is_future<Future>::value,
                        future<when_any_result<std::vector<Future, Alloc>>>> {
  using Sequence = std::vector<Future, Alloc>;
  return {detail::when_any_state<Sequence>::make(std::move(futures))};
}
#endif

} // namespace cxx14_v1
} // namespace portable_concurrency
