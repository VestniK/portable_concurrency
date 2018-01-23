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
struct callable;

template<typename R, typename... A>
struct callable<R(A...)>: move_constructable {
  virtual R call(A... a) = 0;
};

template<typename F, typename S>
struct callable_wrapper;

template<typename F, typename R, typename... A>
struct callable_wrapper<F, R(A...)> final: callable<R(A...)> {
  template<typename U>
  callable_wrapper(U&& val): func{std::forward<U>(val)} {}

  R call(A... a) final {return portable_concurrency::cxx14_v1::detail::invoke(func, a...);}

  move_constructable* move_to(void* location, size_t space) noexcept final {
    void* obj_start = align(alignof(callable_wrapper), sizeof(callable_wrapper), location, space);
    assert(obj_start);
    return new(obj_start) callable_wrapper{std::move(func)};
  }

  F func;
};

template<typename T>
auto is_null(const T&) noexcept
  -> std::enable_if_t<!std::is_convertible<std::nullptr_t, T>::value, bool>
{
  return false;
}

template<typename T>
auto is_null(const T& val) noexcept
  -> std::enable_if_t<std::is_convertible<std::nullptr_t, T>::value, bool>
{
  return val == nullptr;
}

} // namespace detail

template<typename S>
class unique_function;

/**
 * @headerfile portable_concurrency/functional
 * @ingroup functional
 * @brief Move-only type erasure for arbitrary callable object.
 *
 * Implementation of http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2015/n4543.pdf proposal.
 */
template<typename R, typename... A>
class unique_function<R(A...)> {
  using type_erasure_t = detail::type_erasure_owner<sizeof(std::function<R(A...)>), alignof(std::function<R(A...)>)>;
  template<typename F>
  using emplace_t = detail::emplace_t<detail::callable_wrapper<std::decay_t<F>, R(A...)>>;
public:
  /**
   * Creates empty `unique_function` object
   */
  unique_function() noexcept = default;
  /**
   * Creates empty `unique_function` object
   */
  unique_function(std::nullptr_t) noexcept: type_erasure_() {}

  /**
   * Creates `unique_function` holding a function @a f.
   *
   * Result of the expression `INVOKE(f, std::declval<A>()...)` must be convertable to type `R`. Where `INVOKE` is an
   * operation defined in the section 20.9.2 of the C++14 standard with additional overload defined in the section
   * 20.9.4.4.
   *
   * If type `F` is pointer to function, pointer to member function or specialization of the `std::refference_wrapper`
   * class template this constructor is guarantied to store passed function object in a small internal buffer and
   * perform no heap allocations or deallocations.
   */
  template<typename F>
  unique_function(F&& f) {
    if (!detail::is_null(f))
      type_erasure_.emplace(emplace_t<F>{}, std::forward<F>(f));
  }

  /**
   * Destroys any stored function object.
   */
  ~unique_function() = default;

  /**
   * Move @a rhs into newlly created object.
   *
   * @post `rhs` is empty.
   */
  unique_function(unique_function&& rhs) noexcept = default;

  /**
   * Destroy function object stored in this `unique_function` object (if any) and move function object from `rhs`
   * to `*this`.
   *
   * @post `rhs` is empty.
   */
  unique_function& operator= (unique_function&& rhs) noexcept = default;

  /**
   * Calls stored function object with parameters @a args and returns result of the operation. If `this` object is empty
   * throws `std::bad_function_call`.
   */
  R operator() (A... args) {
    if (!type_erasure_.get())
      throw std::bad_function_call{};
    return static_cast<detail::callable<R(A...)>*>(type_erasure_.get())->call(args...);
  }

  /**
   * Checks if this object holds a function (not empty).
   */
  explicit operator bool () const noexcept {
    return type_erasure_.get();
  }

private:
  type_erasure_t type_erasure_;
};

} // inline namespace cxx14_v1
} // namespace portable_concurrency
