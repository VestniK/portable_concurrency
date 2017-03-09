#pragma once

#include <tuple>
#include <type_traits>
#include <utility>

#include "fwd.h"

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

template<typename F>
struct is_future: std::false_type {};

template<typename T>
struct is_future<future<T>>: std::true_type {};

template<typename T>
struct is_future<shared_future<T>>: std::true_type {};

template<typename... F>
struct are_futures;

template<>
struct are_futures<>: std::true_type {};

template<typename F0, typename... F>
struct are_futures<F0, F...>: std::integral_constant<
  bool,
  is_future<F0>::value && are_futures<F...>::value
> {};

template<typename Sequence>
struct find;

template<typename Future>
struct find<std::vector<Future>> {
  static std::size_t index_of_ready(const std::vector<Future>& seq) {
    std::size_t res = 0;
    for (const auto& f: seq) {
      if (f.is_ready())
        return res;
      ++res;
    }
    return static_cast<std::size_t>(-1);
  }
};

template<typename... Futures>
struct find<std::tuple<Futures...>> {
  static std::size_t index_of_ready(const std::tuple<Futures...>& seq) {
    return index_of_ready(seq, std::make_index_sequence<sizeof...(Futures)>{});
  }

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
};

template<typename Sequence>
class when_any_state:
  public shared_state<when_any_result<Sequence>>,
  public continuation
{
public:
  when_any_state(Sequence&& futures): futures_(std::move(futures)) {}

  // threadsafe
  void invoke() override {
    if (satisfied_.exchange(true))
      return;
    this->emplace(when_any_result<Sequence>{
      find<Sequence>::index_of_ready(futures_),
      std::move(futures_)
    });
  }

  Sequence& futures() {return futures_;}

private:
  Sequence futures_;
  std::atomic<bool> satisfied_ = {false};
};

template<typename T>
shared_state<T>* state_of(future<T>& f) {
  return f.state_.get();
}

template<typename T>
shared_state<T>* state_of(shared_future<T>& f) {
  return f.state_.get();
}

void swallow(...) {}

template<typename Sequence, size_t... I>
auto when_any(Sequence&& seq, std::index_sequence<I...>) {
  auto state = std::make_shared<when_any_state<Sequence>>(std::move(seq));
  swallow(
    (state_of(std::get<I>(state->futures()))->add_continuation(state), 0)...
  );
  return future<when_any_result<Sequence>>{state};
}

} // namespace detail

future<when_any_result<std::tuple<>>> when_any() {
  return make_ready_future(when_any_result<std::tuple<>>{
    static_cast<std::size_t>(-1),
    std::tuple<>{}
  });
}

template<typename... Futures>
auto when_any(Futures&&... futures) ->
  std::enable_if_t<
    sizeof...(Futures) != 0 && detail::are_futures<std::decay_t<Futures>...>::value,
    future<when_any_result<std::tuple<std::decay_t<Futures>...>>>
  >
{
  using Sequence = std::tuple<std::decay_t<Futures>...>;
  return detail::when_any(
    Sequence{std::forward<Futures>(futures)...},
    std::make_index_sequence<sizeof...(Futures)>{}
  );
}

} // inline namespace concurrency_v1
} // namespace experimental
