#pragma once

#include <memory>

namespace portable_concurrency {
inline namespace cxx14_v1 {
namespace detail {

#if defined(__GNUC__ ) && __GNUC__ < 5
void* align(std::size_t alignment, std::size_t size, void*& ptr, std::size_t& space );
#else
using std::align;
#endif

} // namespace detail
} // inline namespace cxx14_v1
} // namespace portable_concurrency
