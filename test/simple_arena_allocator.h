#pragma once

#include <portable_concurrency/bits/align.h>

template<typename T, typename Arena>
struct arena_allocator {
  Arena& arena;

  using value_type = T;

  arena_allocator(Arena& arena):
      arena(arena)
  {}

  template<typename U>
  arena_allocator(const arena_allocator<U, Arena>& other):
      arena(other.arena)
  {}

  T* allocate(std::size_t count) {
    return reinterpret_cast<T*>(arena.allocate(sizeof(T) * count, alignof(T)));
  }

  void deallocate(void* ptr, std::size_t size) {}

  template<typename U>
  struct rebind {
    using other = arena_allocator<U, Arena>;
  };
};

template<typename Arena>
struct arena_allocator<void, Arena> {
  Arena& arena;

  using value_type = void;

  arena_allocator(Arena& arena):
      arena(arena)
  {}

  template<typename U>
  arena_allocator(const arena_allocator<U, Arena>& other):
      arena(other.arena)
  {}

  template<typename U>
  struct rebind {
    using other = arena_allocator<U, Arena>;
  };
};

template<typename T, typename Arena>
bool operator== (const arena_allocator<T, Arena>& lhs, const arena_allocator<T, Arena>& rhs) {
  return &lhs.arena == &rhs.arena;
}

template<size_t Size>
class static_arena {
  std::array<uint8_t, Size> data_;
  std::size_t offset_ = 0;
public:
  static_arena() = default;

  void* allocate(std::size_t bytes, std::size_t alignment) {
    void* start = data_.data() + offset_;
    size_t sz = data_.size() - offset_;
    void* result = pc::cxx14_v1::detail::align(alignment, bytes, start, sz);
    if (!result)
      throw std::bad_alloc{};
    offset_ = static_cast<size_t>(reinterpret_cast<std::uint8_t*>(result) - data_.data() + bytes);
    return result;
  }

  std::size_t used() const {
    return data_.size() - offset_;
  }
};
