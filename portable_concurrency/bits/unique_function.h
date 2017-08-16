#pragma once

#include <functional>
#include <memory>
#include <type_traits>

#include "invoke.h"

namespace portable_concurrency {
inline namespace cxx14_v1 {

namespace detail {

template<typename R, typename... A>
class invokable {
public:
  virtual R invoke(A... a) = 0;
};

template<typename F, typename R, typename... A>
class invokable_wrapper final: public invokable<R, A...> {
public:
  invokable_wrapper(F&& f): func_(std::forward<F>(f)) {}
  R invoke(A... a) override {return portable_concurrency::cxx14_v1::detail::invoke(func_, a...);}

private:
  std::decay_t<F> func_;
};

} // namespace detail

template<typename S>
class unique_function;

// Move-only type erasure for arbitrary callable object. Implementation of
// http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2015/n4543.pdf
template<typename R, typename... A>
class unique_function<R(A...)> {
public:
  unique_function() = default;

  template<typename F>
  unique_function(F&& f): func_(std::make_unique<detail::invokable_wrapper<F, R, A...>>(std::forward<F>(f))) {}

  ~unique_function() = default;

  unique_function(unique_function&& rhs) noexcept = default;
  unique_function& operator= (unique_function&& rhs) noexcept = default;

  R operator() (A... args) {
    if (!func_)
      throw std::bad_function_call{};
    return func_->invoke(args...);
  }

private:
  std::unique_ptr<detail::invokable<R, A...>> func_;
};

} // inline namespace cxx14_v1
} // namespace portable_concurrency
