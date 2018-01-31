#pragma once

#include <memory>

namespace portable_concurrency {
inline namespace cxx14_v1 {
namespace detail {

template<typename Alloc>
struct unique_allocator_free {
  Alloc alloc_;
  explicit unique_allocator_free(Alloc&& alloc) :
      alloc_(std::move(alloc))
  { }

  explicit unique_allocator_free(const Alloc& alloc) :
      alloc_(alloc)
  { }

  template<typename T>
  inline void operator()(T* data) {
    std::allocator_traits<Alloc>::destroy(alloc_, data);
    std::allocator_traits<Alloc>::deallocate(alloc_, data, 1);
  }
};

template<typename T, typename Alloc, typename...Args>
inline std::unique_ptr<T, unique_allocator_free<Alloc>> allocate_unique(Alloc allocator, Args&&...args) {
  static_assert(std::is_convertible<typename std::allocator_traits<Alloc>::pointer, T*>::value, "Can't convert pointer from allocator to the item type");
  auto result = std::allocator_traits<Alloc>::allocate(allocator, 1);
  try {
    std::allocator_traits<Alloc>::construct(allocator, result, std::forward<Args>(args)...);
  } catch (...) {
    std::allocator_traits<Alloc>::deallocate(allocator, result, 1);
    throw;
  }

  return std::unique_ptr<T, unique_allocator_free<Alloc>>(result, unique_allocator_free<Alloc>(std::move(allocator)));
};

}
}
}

