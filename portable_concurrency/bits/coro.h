#pragma once

#if defined(__cpp_impl_coroutine)
#if __has_include(<coroutine>)
#include <coroutine>
namespace portable_concurrency {
namespace detail {
inline namespace cxx14_v1 {
using suspend_never = std::suspend_never;
template <typename Promise = void>
using coroutine_handle = std::coroutine_handle<Promise>;
} // namespace cxx14_v1
} // namespace detail
} // namespace portable_concurrency
#define PC_HAS_COROUTINES
#endif
#elif defined(__cpp_coroutines)
#include <experimental/coroutine>
namespace portable_concurrency {
inline namespace cxx14_v1 {
namespace detail {
using suspend_never = std::experimental::suspend_never;
template <typename Promise = void>
using coroutine_handle = std::experimental::coroutine_handle<Promise>;
#define PC_HAS_COROUTINES
} // namespace detail
} // namespace cxx14_v1
} // namespace portable_concurrency
#endif
