#pragma once
#include <atomic>
#include <cassert>
#include <future>

#include "future.h"
#include "utils.h"

namespace experimental {
inline namespace concurrency_v1 {

namespace detail {

#if defined(_MSC_VER)
template<typename F, typename... A>
struct async_res {
  using type = std::result_of_t<std::decay_t<F>(std::decay_t<A>...)>;
};

template<typename F, typename... A>
using async_res_t = typename async_res<F, A...>::type;
#else
template<typename F, typename... A>
using async_res_t = std::result_of_t<std::decay_t<F>(std::decay_t<A>...)>;
#endif

template<typename F, typename... A>
class deferred_action: public action {
public:
  deferred_action(const std::shared_ptr<shared_state<async_res_t<F, A...>>>& state, F&& func, A&&... a):
    state_(state),
    func_(std::forward<F>(func)),
    args_(std::forward<A>(a)...)
  {}

  void invoke() noexcept override {
    bool expected = false;
    if (!executed_.compare_exchange_strong(expected, true))
      return;
    invoke(std::make_index_sequence<sizeof...(A)>());
  }

  bool is_executed() const noexcept override {
    return executed_;
  }

  template<size_t... I>
  void invoke(std::index_sequence<I...>) noexcept {
    auto state = state_.lock();
    if (!state)
      return;
    ::experimental::concurrency_v1::detail::set_state_value(
      *state,
      std::move(func_),
      std::move(std::get<I>(args_))...
    );
  }

private:
  std::weak_ptr<shared_state<async_res_t<F, A...>>> state_;
  std::decay_t<F> func_;
  std::tuple<std::decay_t<A>...> args_;
  std::atomic<bool> executed_ = {false};
};

template<typename F, typename... A>
std::shared_ptr<action> make_deferred_action(
  std::shared_ptr<shared_state<async_res_t<F, A...>>> state,
  F&& f,
  A&&... a
) {
  return std::shared_ptr<action>{
    new deferred_action<F, A...>{state, std::forward<F>(f), std::forward<A>(a)...}
  };
}

} // namespace detail

template<typename F, typename... A>
future<detail::async_res_t<F, A...>> async(std::launch launch, F&& f, A&&... a) {
  if (launch == std::launch::async) {
    throw std::system_error(
      std::make_error_code(std::errc::resource_unavailable_try_again),
      "Futures with blocking destrctor are not supported"
    );
  }

  assert((launch & std::launch::deferred) != static_cast<std::launch>(0));
  auto state = std::make_shared<detail::shared_state<detail::async_res_t<F, A...>>>();
  state->set_deferred_action(
    detail::make_deferred_action(state, std::forward<F>(f), std::forward<A>(a)...)
  );
  return detail::make_future(std::move(state));
}

template<typename F, typename... A>
auto async(F&& f, A&&... a) {
  return async(std::launch::deferred, std::forward<F>(f), std::forward<A>(a)...);
}

} // inline namespace concurrency_v1
} // namespace experimental
