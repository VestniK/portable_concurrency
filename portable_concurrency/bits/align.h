#pragma once

#include <memory>
#include <type_traits>

#include <portable_concurrency/bits/config.h>

namespace portable_concurrency {
inline namespace cxx14_v1 {
namespace detail {

#if !defined(HAS_STD_ALIGN)
void* align(std::size_t alignment, std::size_t size, void*& ptr, std::size_t& space);
#else
using std::align;
#endif

#if !defined(HAS_STD_ALIGNED_UNION)
constexpr auto max(std::size_t a) { return a; }
constexpr auto max(std::size_t a, std::size_t b) { return a > b ? a : b; }
template <typename H, typename... T>
constexpr auto max(H h, T... t) {
  return max(h, max(t...));
}

template <typename... T>
using aligned_union_t = std::aligned_storage_t<max(sizeof(T)...), max(alignof(T)...)>;
#else
template <typename... T>
using aligned_union_t = std::aligned_union_t<1, T...>;
#endif

} // namespace detail
} // namespace cxx14_v1
} // namespace portable_concurrency
