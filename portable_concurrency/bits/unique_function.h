#pragma once

#include <cassert>
#include <functional>
#include <type_traits>

#include "invoke.h"
#include "type_erasure_owner.h"

namespace portable_concurrency {
inline namespace cxx14_v1 {

namespace detail {

template<typename S>
class callable;

template<typename R, typename... A>
class callable<R(A...)> {
public:
  virtual ~callable() = default;
  virtual callable* move_to(char* location) noexcept = 0;
  virtual R call(A... a) = 0;
};

template<typename F, typename S>
class callable_wrapper;

template<typename F, typename R, typename... A>
class callable_wrapper<F, R(A...)> final:
  public move_erased<callable<R(A...)>, callable_wrapper<F, R(A...)>>
{
public:
  callable_wrapper(F&& f): func_(std::forward<F>(f)) {}
  R call(A... a) override {return portable_concurrency::cxx14_v1::detail::invoke(func_, a...);}

private:
  std::decay_t<F> func_;
};

template<template<typename...> class Tmpl, typename T>
struct is_instantiation_of: std::false_type {};
template<template<typename...> class Tmpl, typename... P>
struct is_instantiation_of<Tmpl, Tmpl<P...>>: std::true_type {};

template<typename F>
using is_raw_nullable_functor = std::integral_constant<
  bool,
  std::is_member_function_pointer<F>::value ||
  std::is_member_object_pointer<F>::value ||
  (std::is_pointer<F>::value && std::is_function<std::remove_pointer_t<F>>::value) ||
  is_instantiation_of<std::function, F>::value
>;

template<typename F>
struct is_nullable_functor_ref: std::false_type {};
template<typename F>
struct is_nullable_functor_ref<std::reference_wrapper<F>>:
  is_raw_nullable_functor<std::decay_t<F>>
{};

template<typename F>
using is_nullable_functor = std::integral_constant<
  bool,
  is_raw_nullable_functor<F>::value || is_nullable_functor_ref<F>::value
>;

template<typename F>
bool is_null_func(F&& f) noexcept {
  return f == nullptr;
}

template<typename F>
bool is_null_func(std::reference_wrapper<F> f) noexcept {
  return f.get() == nullptr;
}

} // namespace detail

template<typename S>
class unique_function;

// Move-only type erasure for arbitrary callable object. Implementation of
// http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2015/n4543.pdf
template<typename R, typename... A>
class unique_function<R(A...)> {
  using type_erasure_t = detail::type_erasure_owner<
    detail::callable<R(A...)>,
    sizeof(std::function<R(A...)>),
    alignof(std::function<R(A...)>)
  >;
  template<typename F>
  using emplace_t = detail::emplace_t<detail::callable_wrapper<F, R(A...)>>;
public:
  unique_function() noexcept = default;
  unique_function(std::nullptr_t) noexcept: type_erasure_() {}

  template<typename F>
  unique_function(F&& f):
    unique_function(std::forward<F>(f), detail::is_nullable_functor<std::decay_t<F>>{})
  {}

  ~unique_function() = default;

  unique_function(unique_function&& rhs) noexcept = default;

  unique_function& operator= (unique_function&& rhs) noexcept = default;

  R operator() (A... args) {
    if (!type_erasure_.get())
      throw std::bad_function_call{};
    return type_erasure_.get()->call(args...);
  }

  explicit operator bool () const noexcept {
    return type_erasure_.get();
  }

private:
  template<typename F>
  unique_function(F&& f, std::false_type): type_erasure_(emplace_t<F>{}, std::forward<F>(f)) {}

  template<typename F>
  unique_function(F&& f, std::true_type) {
    if (!detail::is_null_func(f))
      type_erasure_.emplace(emplace_t<F>{}, std::forward<F>(f));
  }

private:
  type_erasure_t type_erasure_;
};

} // inline namespace cxx14_v1
} // namespace portable_concurrency
