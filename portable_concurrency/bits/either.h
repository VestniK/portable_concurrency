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

#if defined(__has_builtin)
#if __has_builtin(__type_pack_element)
#define USE_TYPE_PACK_BUILTIN
#endif
#endif

#if defined(USE_TYPE_PACK_BUILTIN)
template <std::size_t I, typename... T>
using at_t = __type_pack_element<I, T...>;
#else
template <std::size_t I, typename... T> struct at;

template <typename H, typename... T> struct at<0u, H, T...> {
  using type = H;
};

template <std::size_t I, typename H, typename... T>
struct at<I, H, T...> : at<I - 1, T...> {};

template <std::size_t I, typename... T> using at_t = typename at<I, T...>::type;
#endif

template <std::size_t I>
using in_place_index_t = std::integral_constant<std::size_t, I>;

struct monostate {};

struct destroy {
  template <typename T> void operator()(T &t) const { t.~T(); }

  void operator()(monostate) const {}
};

template <typename... T> class either;

// Minimalistic backport of std::variant<std::monostate, T...> from C++17:
// MoveConstructible && !CopyConstructible
//
// Move operaton is explicitly marked as noexcept(true) and only used by library
// for instantiations on types which are nothrow move constructible.
//
// First state (monostate) is used as valueless by exception. Some steps are
// required to shift to std::variant after switch to C++17
template <typename... T> class either<monostate, T...> {
public:
  constexpr static std::size_t empty_state = 0;

  either() noexcept = default;
  ~either() { clean(); }

  either(const either &) = delete;
  either &operator=(const either &) = delete;

  template <std::size_t I, typename... A>
  either(in_place_index_t<I> tag, A &&...a) {
    emplace(tag, std::forward<A>(a)...);
  }

  either(either &&rhs) noexcept {
    move_from(std::move(rhs), std::make_index_sequence<sizeof...(T)>{});
  }
  either &operator=(either &&rhs) noexcept {
    clean();
    move_from(std::move(rhs), std::make_index_sequence<sizeof...(T)>{});
    return *this;
  }

  template <std::size_t I, typename... A>
  void emplace(in_place_index_t<I>, A &&...a) {
    static_assert(I != empty_state, "Can't emplace construct monostate");
    clean();
    new (&storage_) at_t<I - 1, T...>(std::forward<A>(a)...);
    state_ = I;
  }

  std::size_t state() const noexcept { return state_; }

  bool empty() const noexcept { return state_ == empty_state; }

  template <std::size_t I> auto &get(in_place_index_t<I>) noexcept {
    assert(state_ == I);
    return reinterpret_cast<at_t<I - 1, T...> &>(storage_);
  }

  template <std::size_t I> const auto &get(in_place_index_t<I>) const noexcept {
    assert(state_ == I);
    return reinterpret_cast<const at_t<I - 1, T...> &>(storage_);
  }

  template <typename F> decltype(auto) visit(F &&f) {
    return visit_backward(in_place_index_t<sizeof...(T)>{}, std::forward<F>(f));
  }

  template <typename F> decltype(auto) visit(F &&f) const {
    return visit_backward(in_place_index_t<sizeof...(T)>{}, std::forward<F>(f));
  }

  void clean() noexcept {
    visit(destroy{});
    state_ = empty_state;
  }

private:
  template <std::size_t... I>
  void move_from(either &&src, std::index_sequence<I...>) noexcept {
    swallow{(src.state_ == I + 1
                 ? (emplace(in_place_index_t<I + 1>{},
                            std::move(reinterpret_cast<T &>(src.storage_))),
                    false)
                 : false)...};
    src.clean();
  }

  template <typename F, std::size_t I>
  decltype(auto) visit_backward(in_place_index_t<I>, F &&f) {
    if (state_ == I)
      return f(reinterpret_cast<at_t<I - 1, T...> &>(storage_));
    return visit_backward(in_place_index_t<I - 1>{}, std::forward<F>(f));
  }

  template <typename F, std::size_t I>
  decltype(auto) visit_backward(in_place_index_t<I>, F &&f) const {
    if (state_ == I)
      return f(reinterpret_cast<const at_t<I - 1, T...> &>(storage_));
    return visit_backward(in_place_index_t<I - 1>{}, std::forward<F>(f));
  }

  template <typename F>
  decltype(auto) visit_backward(in_place_index_t<empty_state>, F &&f) const {
    return f(monostate{});
  }

  template <typename F>
  decltype(auto) visit_backward(in_place_index_t<empty_state>, F &&f) {
    return f(monostate{});
  }

private:
  std::aligned_union_t<1, T...> storage_;
  std::size_t state_ = empty_state;
};

template <typename T> struct scope_either_cleaner;

template <typename... T> struct scope_either_cleaner<either<monostate, T...>> {
  either<monostate, T...> &target;

  ~scope_either_cleaner() { target.clean(); }
};

} // namespace detail
} // namespace cxx14_v1
} // namespace portable_concurrency
