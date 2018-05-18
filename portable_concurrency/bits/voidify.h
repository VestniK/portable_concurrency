#pragma once

namespace portable_concurrency {
inline namespace cxx14_v1 {
namespace detail {

template<typename T>
struct voidify {using type = void;};

} // namespace detail
} // inline namespace cxx14_v1
} // namespace portable_concurrency
