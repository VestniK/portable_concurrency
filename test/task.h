#pragma once

#include <memory>
#include <tuple>
#include <utility>

#include <portable_concurrency/bits/invoke.h>

template <typename F, typename... A>
class task {
public:
  task(F&& f, A&&... a) : func_(std::forward<F>(f)), args_(std::forward<A>(a)...) {}

  auto operator()() { return run(std::make_index_sequence<sizeof...(A)>()); }

private:
  template <size_t... I>
  auto run(std::index_sequence<I...>) {
    return pc::detail::invoke(std::move(func_), std::move(std::get<I>(args_))...);
  }

  std::decay_t<F> func_;
  std::tuple<std::decay_t<A>...> args_;
};

template <typename F, typename... A>
task<F, A...> make_task(F&& f, A&&... a) {
  return {std::forward<F>(f), std::forward<A>(a)...};
}
