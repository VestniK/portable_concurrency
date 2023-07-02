#pragma once

#include <cstddef>
#include <type_traits>

#include "small_unique_function.h"

namespace portable_concurrency {
inline namespace cxx14_v1 {

template <typename S> class unique_function;

/**
 * @headerfile portable_concurrency/functional
 * @ingroup functional
 * @brief Move-only type erasure for arbitrary callable object.
 *
 * Implementation of
 * http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2015/n4543.pdf proposal.
 *
 * @a portable_concurrency/functional_fwd header provides lightweight forward
 * declarations of this class template public interface allowing to use this
 * class as parameter or return type of function declarations or as member of
 * classes.
 */
template <typename R, typename... A> class unique_function<R(A...)> {
public:
  /**
   * Creates empty `unique_function` object
   */
  unique_function() noexcept;
  /**
   * Creates empty `unique_function` object
   */
  unique_function(std::nullptr_t) noexcept;

  /**
   * Creates `unique_function` holding a function @a f.
   *
   * Result of the expression `INVOKE(f, std::declval<A>()...)` must be
   * convertible to type `R`. Where `INVOKE` is an operation defined in the
   * section 20.9.2 of the C++14 standard with additional overload defined in
   * the section 20.9.4.4.
   *
   * If type `F` is pointer to function, pointer to member function or
   * specialization of the `std::reference_wrapper` class template this
   * constructor is guaranteed to store passed function object in a small
   * internal buffer and perform no heap allocations or deallocations.
   *
   * This implementation additionally provides the small object optimization
   * guarantees for the `packaged_task` class template instantiations and for
   * any function type which is sent to a user provided executor via
   * ADL-discovered function `post`.
   */
  template <typename F, typename = std::enable_if_t<!std::is_same<
                            std::decay_t<F>, unique_function>::value>>
  unique_function(F &&f);

  /**
   * Destroys any stored function object.
   */
  ~unique_function();

  /**
   * Move @a rhs into newly created object.
   */
  unique_function(unique_function &&) noexcept;

  /// Unique function is not CopyConstructible
  unique_function(const unique_function &) = delete;
  /// Unique function is not CopyAssignable
  unique_function &operator=(const unique_function &) = delete;

  /**
   * Destroy function object stored in this `unique_function` object (if any)
   * and move function object from `rhs` to `*this`.
   */
  unique_function &operator=(unique_function &&rhs) noexcept;

  unique_function(detail::small_unique_function<R(A...)> &&rhs) noexcept;
  unique_function &
  operator=(detail::small_unique_function<R(A...)> &&rhs) noexcept;

  operator detail::small_unique_function<R(A...)> &&() &&noexcept;

  /**
   * Calls stored function object with parameters @a args and returns result of
   * the operation. If `this` object is empty throws `std::bad_function_call`.
   */
  R operator()(A... args) const;

  /**
   * Checks if this object holds a function (not empty).
   */
  explicit operator bool() const noexcept { return static_cast<bool>(func_); }

  /**
   * Also checks if this object holds a function
   */
  bool operator==(std::nullptr_t) const noexcept {
    return !static_cast<bool>(func_);
  }

private:
  template <typename F> unique_function(F &&f, std::true_type);

  template <typename F> unique_function(F &&f, std::false_type);

private:
  detail::small_unique_function<R(A...)> func_;
};

extern template class unique_function<void()>;

} // namespace cxx14_v1
} // namespace portable_concurrency
