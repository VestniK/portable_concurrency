#pragma once

#include <cassert>
#include <cstdint>
#include <type_traits>
#include <utility>

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

template<typename T, typename U>
class either {
public:
  constexpr static std::size_t empty_state = 2u;

  either() noexcept = default;
  ~either() {clean();}

  either(const either&) = delete;
  either& operator= (const either&) = delete;

  template<std::size_t I, typename... A>
  either(state_t<I> tag, A&&... a) {
    emplace(tag, std::forward<A>(a)...);
  }

  either(either&& rhs)
#if defined(__GNUC__) && __GNUC__ < 5
    noexcept
#else
    noexcept(are_nothrow_move_constructible<T, U>::value)
#endif
  {
    switch (rhs.state_) {
    case 0u: emplace(state_t<0>{}, std::move(rhs.get(state_t<0>{}))); break;
    case 1u: emplace(state_t<1>{}, std::move(rhs.get(state_t<1>{}))); break;
    }
    rhs.clean();
  }
  either& operator= (either&& rhs)
#if defined(__GNUC__) && __GNUC__ < 5
    noexcept
#else
    noexcept(are_nothrow_move_constructible<T, U>::value)
#endif
  {
    clean();
    switch (rhs.state_) {
    case 0u: emplace(state_t<0>{}, std::move(rhs.get(state_t<0>{}))); break;
    case 1u: emplace(state_t<1>{}, std::move(rhs.get(state_t<1>{}))); break;
    }
    rhs.clean();
    return *this;
  }

  template<std::size_t I, typename... A>
  void emplace(state_t<I>, A&&... a) {
    static_assert(I < empty_state, "Can't emplace construct empty state");
    clean();
    new(&storage_) at_t<I, T, U>(std::forward<A>(a)...);
    state_ = I;
  }

  std::size_t state() const noexcept {return state_;}

  bool empty() const noexcept {return state_ == empty_state;}

  template<std::size_t I>
  auto& get(state_t<I>) noexcept {
    assert(state_ == I);
    return reinterpret_cast<at_t<I, T, U>&>(storage_);
  }

  template<std::size_t I>
  const auto& get(state_t<I>) const noexcept {
    assert(state_ == I);
    return reinterpret_cast<const at_t<I, T, U>&>(storage_);
  }

  void clean() noexcept {
    switch (state_) {
    case 0u: get(state_t<0>{}).~T(); break;
    case 1u: get(state_t<1>{}).~U(); break;
    }
    state_ = empty_state;
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
  std::size_t state_ = empty_state;
};

} // namespace detail
} // inline namespace cxx14_v1
} // namespace portable_concurrency
