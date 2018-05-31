#pragma once

#include <memory>
#include <tuple>
#include <utility>

#include <portable_concurrency/bits/invoke.h>

template<typename F, typename... A>
struct task {
  auto operator() () {
    return run(std::make_index_sequence<sizeof...(A)>());
  }

  template<size_t... I>
  auto run(std::index_sequence<I...>) {
    return pc::detail::invoke(std::move(func_), std::move(std::get<I>(args_))...);
  }

  F func_;
  std::tuple<A...> args_;
};

template<typename F, typename... A>
task<std::decay_t<F>, std::decay_t<A>...> make_task(F&& f, A&&... a) {
  return {std::forward<F>(f), std::forward_as_tuple(std::forward<A>(a)...)};
}
