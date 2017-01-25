#pragma once

#include <cassert>
#include <future>

#include "future.h"
#include "make_future.h"

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
class deferred_shared_state: public shared_state<async_res_t<F, A...>> {
public:
  using result_t = async_res_t<F, A...>;

  deferred_shared_state(F&& f, A&&... a):
    shared_state<result_t>(true),
    func_(std::forward<F>(f)),
    args_(std::forward<A>(a)...)
  {}

protected:
  void deferred_action() noexcept override {
    deferred_action(std::make_index_sequence<sizeof...(A)>());
  }

private:
  template<size_t... I>
  void deferred_action(std::index_sequence<I...>) noexcept try {
    shared_state<result_t>::emplace(
      ::experimental::concurrency_v1::detail::invoke(std::move(func_), std::move(std::get<I>(args_))...)
    );
  } catch (...) {
    shared_state<result_t>::set_exception(std::current_exception());
  }

private:
  std::decay_t<F> func_;
  std::tuple<std::decay_t<A>...> args_;
};

} // namespace detail

template<typename F, typename... A>
future<std::result_of_t<std::decay_t<F>(std::decay_t<A>...)>> async(std::launch launch, F&& f, A&&... a) {
  if (launch == std::launch::async) {
    throw std::system_error(
      std::make_error_code(std::errc::resource_unavailable_try_again),
      "Futures with blocking destrctor are not supported"
    );
  }

  assert((launch & std::launch::deferred) != static_cast<std::launch>(0));
  return detail::make_future(std::shared_ptr<detail::shared_state<detail::async_res_t<F, A...>>>(
    new detail::deferred_shared_state<F, A...>{std::forward<F>(f), std::forward<A>(a)...}
  ));
}

template<typename F, typename... A>
auto async(F&& f, A&&... a) {
  return async(std::launch::deferred, std::forward<F>(f), std::forward<A>(a)...);
}

} // inline namespace concurrency_v1
} // namespace experimental
