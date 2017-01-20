#pragma once

#include <cassert>
#include <future>

#include "future.h"
#include "make_future.h"
#include "postponed_action.h"

namespace experimental {
inline namespace concurrency_v1 {

namespace detail {

template<typename F, typename... A>
struct deffered_action: public erased_action {
  using result_t = std::result_of_t<std::decay_t<F>(std::decay_t<A>...)>;

  deffered_action(std::weak_ptr<shared_state<result_t>>&& state, F&& f, A&&... a):
    weak_state(std::move(state)),
    func(std::forward<F>(f)),
    args(std::forward<A>(a)...)
  {}

  std::weak_ptr<shared_state<result_t>> weak_state;
  std::decay_t<F> func;
  std::tuple<std::decay_t<A>...> args;

  void invoke() override {
    auto state = weak_state.lock();
    if (state)
      invoke(state, std::make_index_sequence<sizeof...(A)>());
  }

private:
  template<size_t... I>
  void invoke(std::shared_ptr<shared_state<result_t>>& state, std::index_sequence<I...>) noexcept try {
    state->emplace(
      ::experimental::concurrency_v1::detail::invoke(std::move(func), std::move(std::get<I>(args))...)
    );
  } catch (...) {
    state->set_exception(std::current_exception());
  }
};

} // namespace detail

template<typename F, typename... A>
future<std::result_of_t<std::decay_t<F>(std::decay_t<A>...)>> async(std::launch launch, F&& f, A&&... a) {
  using R = std::result_of_t<std::decay_t<F>(std::decay_t<A>...)>;
  if (launch == std::launch::async) {
    throw std::system_error(
      std::make_error_code(std::errc::resource_unavailable_try_again),
      "Futures with blocking destrctor are not supported"
    );
  }
  assert((launch & std::launch::deferred) != static_cast<std::launch>(0));

  auto state = std::make_shared<detail::shared_state<R>>();
  state->set_wait_action(
    std::unique_ptr<detail::erased_action>{
      new detail::deffered_action<F, A...>{state, std::forward<F>(f), std::forward<A>(a)...}
    }
  );
  return detail::make_future(std::move(state));
}

template<typename F, typename... A>
auto async(F&& f, A&&... a) {
  return async(std::launch::deferred, std::forward<F>(f), std::forward<A>(a)...);
}

} // inline namespace concurrency_v1
} // namespace experimental
