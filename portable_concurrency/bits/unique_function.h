#pragma once

#include <cstddef>
#include <type_traits>

namespace portable_concurrency {
inline namespace cxx14_v1 {

namespace detail {

constexpr size_t small_buffer_size = 5*sizeof(void*);
constexpr size_t small_buffer_align = alignof(void*);
using small_buffer = std::aligned_storage_t<small_buffer_size, small_buffer_align>;

template<typename R, typename... A>
struct callable_vtbl;

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
  unique_function() noexcept;
  /**
   * Creates empty `unique_function` object
   */
  unique_function(std::nullptr_t) noexcept;

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
  unique_function(F&& f);

  /**
   * Destroys any stored function object.
   */
  ~unique_function();

  /**
   * Move @a rhs into newlly created object.
   *
   * @post `rhs` is empty.
   */
  unique_function(unique_function&& rhs) noexcept;

  /**
   * Destroy function object stored in this `unique_function` object (if any) and move function object from `rhs`
   * to `*this`.
   *
   * @post `rhs` is empty.
   */
  unique_function& operator= (unique_function&& rhs) noexcept;

  /**
   * Calls stored function object with parameters @a args and returns result of the operation. If `this` object is empty
   * throws `std::bad_function_call`.
   *
   * The behaviour is undefined if called on moved from instance.
   */
  R operator() (A... args);

  /**
   * Checks if this object holds a function (not empty).
   */
  explicit operator bool () const noexcept {
    return vtbl_ != nullptr;
  }

  /**
   * Also checks if this object holds a function
   */
  bool operator==(std::nullptr_t) const {
    return vtbl_ == nullptr;
  }

private:
  template<typename F>
  unique_function(F&& f, std::true_type);

  template<typename F>
  unique_function(F&& f, std::false_type);

private:
  detail::small_buffer buffer_;
  const detail::callable_vtbl<R, A...>* vtbl_ = nullptr;
};

extern template class unique_function<void()>;

} // inline namespace cxx14_v1
} // namespace portable_concurrency
