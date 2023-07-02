#pragma once

namespace portable_concurrency {
inline namespace cxx14_v1 {
namespace detail {

// TODO: replace with std::void_t after migration to C++17
template <typename T> struct voidify {
  using type = void;
};

} // namespace detail
} // namespace cxx14_v1
} // namespace portable_concurrency
