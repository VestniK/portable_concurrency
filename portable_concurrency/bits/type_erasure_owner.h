#pragma once

#include <cassert>
#include <algorithm>
#include <cstddef>
#include <memory>
#include <type_traits>

#include "align.h"

namespace portable_concurrency {
inline namespace cxx14_v1 {
namespace detail {

template<typename T>
struct emplace_t {};

/// Small Object Optimized type erased object owner.
template<typename Iface, size_t Len, size_t Align = alignof(void*)>
class type_erasure_owner {
public:
  type_erasure_owner() = default;
  type_erasure_owner(std::nullptr_t): type_erasure_owner() {}

  template<typename T, typename... A>
  type_erasure_owner(emplace_t<T> tag, A&&... a) {
    emplace(tag, std::forward<A>(a)...);
  }

  ~type_erasure_owner() {
    destroy();
  }

  type_erasure_owner(type_erasure_owner&& rhs) noexcept: type_erasure_owner() {
    if (!rhs.is_embeded()) {
      ptr_ = std::exchange(rhs.ptr_, nullptr);
      return;
    }
    ptr_ = rhs.ptr_->move_to(&embeded_buf_, Len);
    rhs.destroy();
  }

  type_erasure_owner& operator= (type_erasure_owner&& rhs) noexcept {
    destroy();
    if (!rhs.is_embeded()) {
      ptr_ = std::exchange(rhs.ptr_, nullptr);
      return *this;
    }
    ptr_ = rhs.ptr_->move_to(reinterpret_cast<char*>(&embeded_buf_), Len);
    rhs.destroy();
    return *this;
  }

  template<typename T, typename... A>
  void emplace(emplace_t<T>, A&&... a) {
    destroy();
    static_assert(std::is_base_of<Iface, T>::value, "T must be derived from Iface");

    constexpr size_t max_align_offset = (alignof(T) - Align%alignof(T))%alignof(T);
    void* ptr = &embeded_buf_;
    size_t space = Len;
    void* obj_start = align(alignof(T), sizeof(T), ptr, space);
    if (
      std::is_nothrow_move_constructible<T>::value &&
      std::is_nothrow_destructible<T>::value &&
      sizeof(T) + max_align_offset <= Len &&
      obj_start
    )
      ptr_ = new(obj_start) T{std::forward<A>(a)...};
    else
      ptr_ = new T{std::forward<A>(a)...};
  }

  Iface* get() const noexcept {
    return ptr_;
  }

  bool is_embeded() const noexcept {
    const void* const storage_start = &embeded_buf_;
    const void* const storage_end = reinterpret_cast<const char*>(&embeded_buf_) + Len;
    const void* const obj_loc = ptr_;
    // HINT builtin comparision operators do not establish total ordering for pointers and it's UB to compare pointers
    // to unrelated objects. std::less and other std comparators however provide consistent total order for pointers
    // to make them be usable in ordered containers like std::set and std::map. Using std::less to prevent UB.
    const std::less<const void*> cmp{};
    return !cmp(obj_loc, storage_start) && cmp(obj_loc, storage_end);
  }

private:
  void destroy() noexcept {
    if (!ptr_)
      return;
    if (is_embeded())
    {
      ptr_->~Iface();
    } else {
      delete ptr_;
    }
    ptr_ = nullptr;
  }

private:
  Iface* ptr_ = nullptr;
  std::aligned_storage_t<Len, Align> embeded_buf_;
};

/**
 * CRTP helper to erase move constructor to make class usable with type_erasure_owner
 */
template<typename Iface, typename Impl>
class move_erased: public Iface {
public:
  Iface* move_to(void* location, size_t space) noexcept final {
    void* obj_start = align(alignof(Impl), sizeof(Impl), location, space);
    assert(obj_start);
    return new(obj_start) Impl{std::move(*static_cast<Impl*>(this))};
  }
};

} // namespace detail
} // inline namespace cxx14_v1
} // namespace portable_concurrency
