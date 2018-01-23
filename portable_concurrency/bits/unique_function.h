#pragma once

#include <cassert>
#include <functional>
#include <type_traits>

#include "invoke.h"

namespace portable_concurrency {
inline namespace cxx14_v1 {

namespace detail {

constexpr size_t small_buffer_size = 5*sizeof(void*);
constexpr size_t small_buffer_align = alignof(void*);
using small_buffer = std::aligned_storage_t<small_buffer_size, small_buffer_align>;

template<typename S>
struct callable;

template<typename R, typename... A>
struct callable<R(A...)> {
  virtual ~callable() = default;
  virtual void move_to(small_buffer& dest) noexcept = 0;
  virtual R call(A... a) {throw std::bad_function_call{};}
  virtual bool is_null() const noexcept {return true;}
};

template<typename S>
struct null_callable final: callable<S> {
  void move_to(small_buffer& dest) noexcept {
    static_assert(alignof(null_callable) <= small_buffer_align, "Object can't be aligned in the buffer properly");
    static_assert(sizeof(null_callable) <= small_buffer_size, "Object too big");
    new(&dest) null_callable{};
  }
};

template<typename F, typename S>
struct callable_wrapper;

template<typename F, typename R, typename... A>
struct callable_wrapper<F, R(A...)> final: callable<R(A...)> {
  template<typename U>
  callable_wrapper(U&& val): func{std::forward<U>(val)} {}

  R call(A... a) final {return portable_concurrency::cxx14_v1::detail::invoke(func, a...);}

  void move_to(small_buffer& dest) noexcept final {
    static_assert(alignof(callable_wrapper) <= small_buffer_align, "Object can't be aligned in the buffer properly");
    static_assert(sizeof(callable_wrapper) <= small_buffer_size, "Object too big");
    new(&dest) callable_wrapper{std::move(func)};
  }

  bool is_null() const noexcept final {return false;}

  F func;
};

template<typename F, typename R, typename... A>
struct callable_wrapper<std::unique_ptr<F>, R(A...)> final: callable<R(A...)> {
  template<typename U>
  callable_wrapper(U&& val): func{std::make_unique<F>(std::forward<U>(val))} {}
  callable_wrapper(std::unique_ptr<F>&& val): func{std::move(val)} {}

  R call(A... a) final {return portable_concurrency::cxx14_v1::detail::invoke(*func, a...);}

  void move_to(small_buffer& dest) noexcept final {
    static_assert(alignof(callable_wrapper) <= small_buffer_align, "Object can't be aligned in the buffer properly");
    static_assert(sizeof(callable_wrapper) <= small_buffer_size, "Object too big");
    new(&dest) callable_wrapper{std::move(func)};
  }

  bool is_null() const noexcept final {return false;}

  std::unique_ptr<F> func;
};

template<typename F, typename S>
using is_storable_t = std::integral_constant<bool,
  alignof(callable_wrapper<F, S>) <= small_buffer_align &&
  sizeof(callable_wrapper<F, S>) <= small_buffer_size
>;

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
public:
  /**
   * Creates empty `unique_function` object
   */
  unique_function() noexcept {new(&buffer_) detail::null_callable<R(A...)>;}
  /**
   * Creates empty `unique_function` object
   */
  unique_function(std::nullptr_t) noexcept: unique_function() {}

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
  unique_function(F&& f): unique_function(std::forward<F>(f), detail::is_storable_t<std::decay_t<F>, R(A...)>{}) {}

  /**
   * Destroys any stored function object.
   */
  ~unique_function() {
    using F = detail::callable<R(A...)>;
    func().~F();
  }

  /**
   * Move @a rhs into newlly created object.
   *
   * @post `rhs` is empty.
   */
  unique_function(unique_function&& rhs) noexcept {
    rhs.func().move_to(buffer_);
  }

  /**
   * Destroy function object stored in this `unique_function` object (if any) and move function object from `rhs`
   * to `*this`.
   *
   * @post `rhs` is empty.
   */
  unique_function& operator= (unique_function&& rhs) noexcept {
    using F = detail::callable<R(A...)>;
    func().~F();
    rhs.func().move_to(buffer_);
    return *this;
  }

  /**
   * Calls stored function object with parameters @a args and returns result of the operation. If `this` object is empty
   * throws `std::bad_function_call`.
   */
  R operator() (A... args) {
    return func().call(args...);
  }

  /**
   * Checks if this object holds a function (not empty).
   */
  explicit operator bool () const noexcept {
    return !func().is_null();
  }

private:
  template<typename F>
  unique_function(F&& f, std::true_type) {
    if (detail::is_null(f))
      new(&buffer_) detail::null_callable< R(A...)>;
    else
      new(&buffer_) detail::callable_wrapper<std::decay_t<F>, R(A...)>{std::forward<F>(f)};
  }

  template<typename F>
  unique_function(F&& f, std::false_type) {
    if (detail::is_null(f))
      new(&buffer_) detail::null_callable< R(A...)>;
    else
      new(&buffer_) detail::callable_wrapper<std::unique_ptr<std::decay_t<F>>, R(A...)>{
       std::make_unique<F>(std::forward<F>(f))
      };
  }

  detail::callable<R(A...)>& func() {return reinterpret_cast<detail::callable<R(A...)>&>(buffer_);}
  const detail::callable<R(A...)>& func() const {return reinterpret_cast<const detail::callable<R(A...)>&>(buffer_);}

private:
  detail::small_buffer buffer_;
};

} // inline namespace cxx14_v1
} // namespace portable_concurrency
