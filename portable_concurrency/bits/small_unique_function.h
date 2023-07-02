#pragma once

#include <cstddef>
#include <type_traits>

namespace portable_concurrency {
inline namespace cxx14_v1 {
namespace detail {

constexpr size_t small_buffer_size = 5 * sizeof(void *);
constexpr size_t small_buffer_align = alignof(void *);
using small_buffer =
    std::aligned_storage_t<small_buffer_size, small_buffer_align>;

template <typename R, typename... A> struct callable_vtbl;

template <typename S> class small_unique_function;

// Move-only type erasure for small NothrowMoveConstructible Callable object.
template <typename R, typename... A> class small_unique_function<R(A...)> {
public:
  small_unique_function() noexcept;
  small_unique_function(std::nullptr_t) noexcept;

  template <typename F> small_unique_function(F &&f);

  ~small_unique_function();

  small_unique_function(const small_unique_function &) = delete;
  small_unique_function &operator=(const small_unique_function &) = delete;

  small_unique_function(small_unique_function &&rhs) noexcept;

  small_unique_function &operator=(small_unique_function &&rhs) noexcept;

  R operator()(A... args) const;

  explicit operator bool() const noexcept { return vtbl_ != nullptr; }

private:
  mutable small_buffer buffer_;
  const callable_vtbl<R, A...> *vtbl_ = nullptr;
};

extern template class small_unique_function<void()>;

} // namespace detail
} // namespace cxx14_v1
} // namespace portable_concurrency
