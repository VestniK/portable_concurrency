#pragma once

#include <cassert>
#include <cstdint>
#include <type_traits>
#include <utility>

#include <portable_concurrency/bits/config.h>

#include "concurrency_type_traits.h"

namespace portable_concurrency {
inline namespace cxx14_v1 {
namespace detail {

template<typename... T>
struct are_nothrow_move_constructible;

template<typename T>
struct are_nothrow_move_constructible<T>: std::is_nothrow_move_constructible<T> {};

template<typename H, typename... T>
struct are_nothrow_move_constructible<H, T...>: std::integral_constant<bool,
  std::is_nothrow_move_constructible<H>::value &&
  are_nothrow_move_constructible<T...>::value
> {};

template<std::size_t I, typename... T>
struct at;

template<typename H, typename... T>
struct at<0u, H, T...> {using type = H;};

template<std::size_t I, typename H, typename... T>
struct at<I, H, T...>: at<I - 1, T...> {};

template<std::size_t I, typename... T>
using at_t = typename at<I, T...>::type;

template<std::size_t I>
using state_t = std::integral_constant<std::size_t, I>;

template<typename... T>
class either {
public:
  constexpr static std::size_t empty_state = sizeof...(T);

  either() noexcept = default;
  ~either() {clean();}

  either(const either&) = delete;
  either& operator= (const either&) = delete;

  template<std::size_t I, typename... A>
  either(state_t<I> tag, A&&... a) {
    emplace(tag, std::forward<A>(a)...);
  }

  either(either&& rhs) NOEXCEPT_IF(are_nothrow_move_constructible<T...>::value) {
    move_from(std::move(rhs), std::make_index_sequence<sizeof...(T)>{});
  }
  either& operator= (either&& rhs) NOEXCEPT_IF(are_nothrow_move_constructible<T...>::value) {
    clean();
    move_from(std::move(rhs), std::make_index_sequence<sizeof...(T)>{});
    return *this;
  }

  template<std::size_t I, typename... A>
  void emplace(state_t<I>, A&&... a) {
    static_assert(I < empty_state, "Can't emplace construct empty state");
    clean();
    new(&storage_) at_t<I, T...>(std::forward<A>(a)...);
    state_ = I;
  }

  std::size_t state() const noexcept {return state_;}

  bool empty() const noexcept {return state_ == empty_state;}

  template<std::size_t I>
  auto& get(state_t<I>) noexcept {
    assert(state_ == I);
    return reinterpret_cast<at_t<I, T...>&>(storage_);
  }

  template<std::size_t I>
  const auto& get(state_t<I>) const noexcept {
    assert(state_ == I);
    return reinterpret_cast<const at_t<I, T...>&>(storage_);
  }

  void clean() noexcept {
    clean(std::make_index_sequence<sizeof...(T)>{});
    state_ = empty_state;
  }

private:
  template<std::size_t... I>
  void move_from(either&& src, std::index_sequence<I...>) {
    swallow{
      (src.state_ == I ? (emplace(state_t<I>{}, std::move(src.get(state_t<I>{}))), false) : false)...
    };
    src.clean();
  }

  template<std::size_t... I>
  void clean(std::index_sequence<I...>) {
    swallow{
      (state_ == I ? (get(state_t<I>{}).~at_t<I, T...>(), false) : false)...
    };
    state_ = empty_state;
  }

private:
#if defined(HAS_STD_ALIGNED_UNION)
  std::aligned_union_t<1, T...> storage_;
#else
  std::aligned_storage_t<
    (sizeof(T) > sizeof(U) ? sizeof(T) : sizeof(U)),
    (alignof(T) > alignof(U) ? alignof(T) : alignof(U))
  > storage_;
#endif
  std::size_t state_ = empty_state;
};

} // namespace detail
} // inline namespace cxx14_v1
} // namespace portable_concurrency
