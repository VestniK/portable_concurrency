#pragma once

#include <type_traits>

#include "invoke.h"
#include "small_unique_function.h"
#include "voidify.h"

namespace portable_concurrency {
inline namespace cxx14_v1 {
namespace detail {

[[noreturn]] void throw_bad_func_call();

// R(A...) is incomplete type so it is illegal to use sizeof(F)/alignof(F) for
// decayed function reference to turn it into function pointer which is
// implicitly constructible from function reference
template <typename F>
using is_storable_helper =
    std::conditional_t<std::is_function<F>::value, F *, F>;

template <typename F>
using is_storable_t = std::integral_constant<
    bool, alignof(is_storable_helper<F>) <= small_buffer_align &&
              sizeof(is_storable_helper<F>) <= small_buffer_size &&
              std::is_nothrow_move_constructible<F>::value>;

template <typename R, typename... A> using func_ptr_t = R (*)(A...);

template <typename R, typename... A> struct callable_vtbl {
  func_ptr_t<void, small_buffer &> destroy;
  func_ptr_t<void, small_buffer &, small_buffer &> move;
  func_ptr_t<R, small_buffer &, A...> call;
};

template <typename F, typename R, typename... A>
const callable_vtbl<R, A...> &get_callable_vtbl() {
  static_assert(is_storable_t<F>::value,
                "Can't embed object into small buffer");
  static const callable_vtbl<R, A...> res = {
      [](small_buffer &buf) { reinterpret_cast<F &>(buf).~F(); },
      [](small_buffer &src, small_buffer &dst) {
        new (&dst) F{std::move(reinterpret_cast<F &>(src))};
      },
      [](small_buffer &buf, A... a) -> R {
#if !defined(_MSC_VER)
        // Must not perform conversions marked as explicit but must cast
        // anything to `void` if `R` is `void`
        return static_cast<std::conditional_t<
            std::is_void<R>::value, void,
            decltype(portable_concurrency::cxx14_v1::detail::invoke(
                reinterpret_cast<F &>(buf), std::forward<A>(a)...))>>(
            portable_concurrency::cxx14_v1::detail::invoke(
                reinterpret_cast<F &>(buf), std::forward<A>(a)...));
#else
        return static_cast<R>(portable_concurrency::cxx14_v1::detail::invoke(
            reinterpret_cast<F &>(buf), std::forward<A>(a)...));
#endif
      }};
  return res;
}

template <typename T, typename = void>
struct is_null_comparable : std::false_type {};

template <typename T>
struct is_null_comparable<
    T, typename voidify<decltype(std::declval<T>() == nullptr)>::type>
    : std::true_type {};

template <typename T>
auto is_null(const T &) noexcept
    -> std::enable_if_t<!is_null_comparable<T>::value, bool> {
  return false;
}

#if defined(__GNUC__) && !defined(__clang__) && __GNUC__ >= 6
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Waddress"
#pragma GCC diagnostic ignored "-Wnonnull-compare"
#endif
template <typename T>
auto is_null(const T &val) noexcept
    -> std::enable_if_t<is_null_comparable<T>::value, bool> {
  return val == nullptr;
}
#if defined(__GNUC__) && !defined(__clang__) && __GNUC__ >= 6
#pragma GCC diagnostic pop
#endif

template <typename R, typename... A>
small_unique_function<R(A...)>::small_unique_function() noexcept = default;

template <typename R, typename... A>
small_unique_function<R(A...)>::small_unique_function(std::nullptr_t) noexcept {
}

template <typename R, typename... A>
template <typename F>
small_unique_function<R(A...)>::small_unique_function(F &&f) {
  static_assert(is_storable_t<std::decay_t<F>>::value,
                "Can't embed object into small_unique_function");
  if (detail::is_null(f))
    return;
  new (&buffer_) std::decay_t<F>{std::forward<F>(f)};
  vtbl_ = &detail::get_callable_vtbl<std::decay_t<F>, R, A...>();
}

template <typename R, typename... A>
small_unique_function<R(A...)>::~small_unique_function() {
  if (vtbl_)
    vtbl_->destroy(buffer_);
}

template <typename R, typename... A>
small_unique_function<R(A...)>::small_unique_function(
    small_unique_function<R(A...)> &&rhs) noexcept {
  if (rhs.vtbl_)
    rhs.vtbl_->move(rhs.buffer_, buffer_);
  vtbl_ = rhs.vtbl_;
}

template <typename R, typename... A>
small_unique_function<R(A...)> &small_unique_function<R(A...)>::operator=(
    small_unique_function<R(A...)> &&rhs) noexcept {
  if (vtbl_)
    vtbl_->destroy(buffer_);
  if (rhs.vtbl_)
    rhs.vtbl_->move(rhs.buffer_, buffer_);
  vtbl_ = rhs.vtbl_;
  return *this;
}

template <typename R, typename... A>
R small_unique_function<R(A...)>::operator()(A... args) const {
  if (!vtbl_)
    throw_bad_func_call();
  return vtbl_->call(buffer_, std::forward<A>(args)...);
}

} // namespace detail
} // namespace cxx14_v1
} // namespace portable_concurrency
