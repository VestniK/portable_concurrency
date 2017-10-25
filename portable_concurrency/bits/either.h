#pragma once

#include <cassert>
#include <cstdint>
#include <type_traits>

#include "align.h"

namespace portable_concurrency {
inline namespace cxx14_v1 {
namespace detail {

enum class either_state: uint8_t {empty, first, second};

template<either_state State>
using either_state_t = std::integral_constant<either_state, State>;
using first_t = either_state_t<either_state::first>;
using second_t = either_state_t<either_state::second>;

template<typename T, typename U>
class either {
public:
  either() noexcept = default;
  ~either() {clean();}

  either(const either&) = delete;
  either& operator= (const either&) = delete;

  template<either_state State, typename... A>
  either(std::integral_constant<either_state, State> tag, A&&... a) {
    emplace(tag, std::forward<A>(a)...);
  }

  either(either&& rhs) noexcept(
    std::is_nothrow_move_constructible<T>::value &&
    std::is_nothrow_move_constructible<U>::value
  ) {
    switch (rhs.state_) {
    case either_state::empty: return;
    case either_state::first: emplace(first_t{}, std::move(rhs.get(first_t{}))); break;
    case either_state::second: emplace(second_t{}, std::move(rhs.get(second_t{}))); break;
    }
    rhs.clean();
  }

  template<typename... A>
  void emplace(first_t tag, A&&... a) {
    clean();
    new(storage(tag)) T{std::forward<A>(a)...};
    state_ = first_t::value;
  }

  template<typename... A>
  void emplace(second_t tag, A&&... a) {
    clean();
    new(storage(tag)) U{std::forward<A>(a)...};
    state_ = second_t::value;
  }

  either_state state() const noexcept {return state_;}

  template<either_state State>
  auto& get(either_state_t<State> tag) noexcept {
    assert(state_ == State);
    return *storage(tag);
  }

private:
  void clean() noexcept {
    switch (state_) {
    case either_state::empty: return;
    case either_state::first: storage(first_t{})->~T(); break;
    case either_state::second: storage(second_t{})->~U(); break;
    }
    state_ = either_state::empty;
  }

  T* storage(first_t) noexcept {
    void* storage = &storage_;
    size_t sz = sizeof(storage_);
    return reinterpret_cast<T*>(detail::align(alignof(T), sizeof(T), storage, sz));
  }

  U* storage(second_t) noexcept {
    void* storage = &storage_;
    size_t sz = sizeof(storage_);
    return reinterpret_cast<U*>(detail::align(alignof(U), sizeof(U), storage, sz));
  }

private:
#if defined(__GNUC__) && __GNUC__ < 5
  std::aligned_storage_t<
    (sizeof(T) > sizeof(U) ? sizeof(T) : sizeof(U)),
    (alignof(T) > alignof(U) ? alignof(T) : alignof(U))
  > storage_;
#else
  std::aligned_union_t<1, T, U> storage_;
#endif
  either_state state_ = either_state::empty;
};

} // namespace detail
} // inline namespace cxx14_v1
} // namespace portable_concurrency
