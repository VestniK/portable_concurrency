#pragma once

#include <tuple>
#include <utility>

#include <portable_concurrency/bits/alias_namespace.h>
#include <portable_concurrency/bits/invoke.h>

template<typename F, typename... A>
struct task {
  auto operator() () {
    return run(std::make_index_sequence<sizeof...(A)>());
  }

  template<size_t... I>
  auto run(std::index_sequence<I...>) {
    return pc::detail::invoke(std::move(func), std::move(std::get<I>(args))...);
  }

  F func;
  std::tuple<A...> args;
};
