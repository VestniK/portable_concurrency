#pragma once

#include <memory>

namespace portable_concurrency {
inline namespace cxx14_v1 {
namespace detail {

template<typename Alloc>
struct allocator_deleter: Alloc {
  Alloc& get_allocator() {return *this;}

  explicit allocator_deleter(Alloc&& alloc) :
      Alloc(std::move(alloc))
  { }

  explicit allocator_deleter(const Alloc& alloc) :
      Alloc(alloc)
  { }

  template<typename T>
  void operator()(T* data) {
    std::allocator_traits<Alloc>::destroy(get_allocator(), data);
    std::allocator_traits<Alloc>::deallocate(get_allocator(), data, 1);
  }
};

template<typename T, typename Alloc, typename... Args>
inline std::unique_ptr<T, allocator_deleter<Alloc>> allocate_unique(Alloc allocator, Args&&... args) {
  static_assert(
    std::is_convertible<typename std::allocator_traits<Alloc>::pointer, T*>::value,
    "Can't convert pointer from allocator to the item type"
  );
  auto result = std::allocator_traits<Alloc>::allocate(allocator, 1);
  try {
    std::allocator_traits<Alloc>::construct(allocator, result, std::forward<Args>(args)...);
  } catch (...) {
    std::allocator_traits<Alloc>::deallocate(allocator, result, 1);
    throw;
  }

  return std::unique_ptr<T, allocator_deleter<Alloc>>(result, allocator_deleter<Alloc>(std::move(allocator)));
};

} // namespace detail
} // inline namespace cxx14_v1
} // namespace portable_concurrency
