#pragma once

#include <algorithm>
#include <cstddef>
#include <type_traits>

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
  type_erasure_owner(emplace_t<T>, A&&... a) {
    static_assert(std::is_base_of<Iface, T>::value, "T must be derived from Iface");

    constexpr size_t align_offset = (alignof(T) - Align%alignof(T))%alignof(T);
    if (
      std::is_nothrow_move_constructible<T>::value &&
      std::is_nothrow_destructible<T>::value &&
      sizeof(T) > Len - align_offset
    )
      ptr_ = new T{std::forward<A>(a)...};
    else {
      char* obj_start = reinterpret_cast<char*>(&embeded_buf_) + align_offset;
      ptr_ = new(obj_start) T{std::forward<A>(a)...};
    }
  }

  ~type_erasure_owner() {
    destroy();
  }

  type_erasure_owner(type_erasure_owner&& rhs) noexcept: type_erasure_owner() {
    if (!rhs.is_embeded()) {
      ptr_ = std::exchange(rhs.ptr_, nullptr);
      return;
    }
    ptr_ = rhs.ptr_->move_to(
      reinterpret_cast<char*>(&embeded_buf_) +
      (reinterpret_cast<char*>(rhs.ptr_) - reinterpret_cast<char*>(&rhs.embeded_buf_))
    );
    rhs.destroy();
  }

  type_erasure_owner& operator= (type_erasure_owner&& rhs) {
    destroy();
    if (!rhs.is_embeded()) {
      ptr_ = std::exchange(rhs.ptr_, nullptr);
      return *this;
    }
    ptr_ = rhs.ptr_->move_to(
      reinterpret_cast<char*>(&embeded_buf_) + (rhs.ptr_ - reinterpret_cast<char*>(&rhs.embeded_buf_))
    );
    rhs.destroy();
    return *this;
  }

  Iface* get() const {
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

#if defined(_MSC_VER)
template<typename Iface, template<typename> class Adapter, typename... T>
struct type_erasure_owner_t_helper {
#if _MSC_VER > 1900
  static constexpr size_t storage_size = std::max(std::initializer_list<size_t>{sizeof(Adapter<T>)...});
  static constexpr size_t storage_align = std::max(std::initializer_list<size_t>{alignof(Adapter<T>)...});
#else
  constexpr static size_t max_sz(size_t sz) {return sz;}

  template<typename... Size_t>
  constexpr static size_t max_sz(size_t sz, Size_t... szs) {
    return sz > max_sz(szs...) ? sz : max_sz(szs...);
  }

  static constexpr size_t storage_size = max_sz(sizeof(Adapter<T>)...);
  static constexpr size_t storage_align = max_sz(alignof(Adapter<T>)...);
#endif
  using type = type_erasure_owner<Iface, storage_size, storage_align>;
};

template<typename Iface, template<typename> class Adapter, typename... T>
using type_erasure_owner_t = typename type_erasure_owner_t_helper<Iface, Adapter, T...>::type;
#else
template<typename Iface, template<typename> class Adapter, typename... T>
using type_erasure_owner_t = type_erasure_owner<
  Iface,
  std::max({sizeof(Adapter<T>)...}),
  std::max({alignof(Adapter<T>)...})
>;
#endif

/**
 * CRTP helper to erase move constructor to make class usable with type_erasure_owner
 */
template<typename Iface, typename Impl>
class move_erased: public Iface {
public:
  Iface* move_to(char* location) noexcept override {
    return new(location) Impl{std::move(*static_cast<Impl*>(this))};
  }
};

} // namespace detail
} // inline namespace cxx14_v1
} // namespace portable_concurrency
