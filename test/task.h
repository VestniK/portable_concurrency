#pragma once

#include <memory>
#include <tuple>
#include <utility>

#include "../concurrency/bits/invoke.h"

class task {
public:
  task() = default;

  template<typename F, typename... A>
  task(F&& f, A&&... a):
    type_erasure_(make_type_erasure(std::forward<F>(f), std::forward<A>(a)...))
  {}

  void operator() () {
    if (!type_erasure_)
      throw std::bad_function_call();
    type_erasure_->run();
    type_erasure_.reset();
  }

private:
  struct type_erasure_base {
    virtual ~type_erasure_base() = default;
    virtual void run() = 0;
  };

  template<typename F, typename... A>
  struct type_erasure: type_erasure_base {
    type_erasure(F&& f, A&&... a):
      f(std::forward<F>(f)),
      args(std::forward<A>(a)...)
    {}

    void run() override {
      run(std::make_index_sequence<sizeof...(A)>());
    }

    template<size_t... I>
    void run(std::index_sequence<I...>) {
      experimental::detail::invoke(std::move(f), std::move(std::get<I>(args))...);
    }

    std::decay_t<F> f;
    std::tuple<std::decay_t<A>...> args;
  };

  template<typename F, typename... A>
  type_erasure_base* make_type_erasure(F&& f, A&&... a) {
    return new type_erasure<F, A...>(std::forward<F>(f), std::forward<A>(a)...);
  }

  std::unique_ptr<type_erasure_base> type_erasure_;
};
