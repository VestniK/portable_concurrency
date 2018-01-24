#pragma once

#include <functional>
#include <memory>
#include <type_traits>

#include "invoke.h"

namespace portable_concurrency {
inline namespace cxx14_v1 {

namespace detail {

constexpr size_t small_buffer_size = 5*sizeof(void*);
constexpr size_t small_buffer_align = alignof(void*);
using small_buffer = std::aligned_storage_t<small_buffer_size, small_buffer_align>;

template<typename F>
using is_storable_t = std::integral_constant<bool,
  alignof(F) <= small_buffer_align &&
  sizeof(F) <= small_buffer_size &&
  std::is_nothrow_move_constructible<F>::value
>;

template<typename R, typename... A>
using func_ptr_t = R (*) (A...);

template<typename R, typename... A>
struct callable_vtbl {
  func_ptr_t<void, small_buffer&> destroy;
  func_ptr_t<void, small_buffer&, small_buffer&> move;
  func_ptr_t<R, small_buffer&, A...> call;
};

template<typename F, typename R, typename... A>
const callable_vtbl<R, A...>& get_callable_vtbl() {
  static_assert (is_storable_t<F>::value, "Can't embed object into small buffer");
  static const callable_vtbl<R, A...> res = {
    [](small_buffer& buf) {reinterpret_cast<F&>(buf).~F();},
    [](small_buffer& src, small_buffer& dst) {new(&dst) F{std::move(reinterpret_cast<F&>(src))};},
    [](small_buffer& buf, A... a) -> R {
      return portable_concurrency::cxx14_v1::detail::invoke(reinterpret_cast<F&>(buf), a...);
    }
  };
  return res;
}

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
  unique_function() noexcept = default;
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
  unique_function(F&& f): unique_function(std::forward<F>(f), detail::is_storable_t<std::decay_t<F>>{}) {}

  /**
   * Destroys any stored function object.
   */
  ~unique_function() {
    if (vtbl_)
      vtbl_->destroy(buffer_);
  }

  /**
   * Move @a rhs into newlly created object.
   *
   * @post `rhs` is empty.
   */
  unique_function(unique_function&& rhs) noexcept {
    if (rhs.vtbl_)
      rhs.vtbl_->move(rhs.buffer_, buffer_);
    vtbl_ = rhs.vtbl_;
  }

  /**
   * Destroy function object stored in this `unique_function` object (if any) and move function object from `rhs`
   * to `*this`.
   *
   * @post `rhs` is empty.
   */
  unique_function& operator= (unique_function&& rhs) noexcept {
    if (vtbl_)
      vtbl_->destroy(buffer_);
    if (rhs.vtbl_)
      rhs.vtbl_->move(rhs.buffer_, buffer_);
    vtbl_ = rhs.vtbl_;
    return *this;
  }

  /**
   * Calls stored function object with parameters @a args and returns result of the operation. If `this` object is empty
   * throws `std::bad_function_call`.
   *
   * The behaviour is undefined if called on moved from instance.
   */
  R operator() (A... args) {
    if (!vtbl_)
      throw std::bad_function_call{};
    return vtbl_->call(buffer_, args...);
  }

  /**
   * Checks if this object holds a function (not empty).
   */
  explicit operator bool () const noexcept {
    return vtbl_ != nullptr;
  }

private:
  template<typename F>
  unique_function(F&& f, std::true_type) {
    if (detail::is_null(f))
      return;
    new(&buffer_) std::decay_t<F>{std::forward<F>(f)};
    vtbl_ = &detail::get_callable_vtbl<std::decay_t<F>, R, A...>();
  }

  template<typename F>
  unique_function(F&& f, std::false_type) {
    if (detail::is_null(f))
      return;
    auto wrapped_func = [func = std::make_unique<std::decay_t<F>>(std::forward<F>(f))](A... a) {
      return detail::invoke(*func, a...);
    };
    using wrapped_func_t = decltype(wrapped_func);
    new(&buffer_) wrapped_func_t{std::move(wrapped_func)};
    vtbl_ = &detail::get_callable_vtbl<wrapped_func_t, R, A...>();
  }

private:
  detail::small_buffer buffer_;
  const detail::callable_vtbl<R, A...>* vtbl_ = nullptr;
};

} // inline namespace cxx14_v1
} // namespace portable_concurrency
