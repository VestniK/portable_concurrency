#pragma once

#include <type_traits>
#include <utility>

namespace portable_concurrency {
inline namespace cxx14_v1 {
namespace detail {

template <typename T> struct is_reference_wrapper : std::false_type {};
template <typename U>
struct is_reference_wrapper<std::reference_wrapper<U>> : std::true_type {};

template <typename B, typename T, typename D, typename... A>
auto invoke(T B::*pmf, D &&ref, A &&...args) noexcept(
    noexcept((std::forward<D>(ref).*pmf)(std::forward<A>(args)...)))
    -> std::enable_if_t<std::is_function<T>::value &&
                            std::is_base_of<B, std::decay_t<D>>::value,
                        decltype((std::forward<D>(ref).*
                                  pmf)(std::forward<A>(args)...))> {
  return (std::forward<D>(ref).*pmf)(std::forward<A>(args)...);
}

template <typename B, typename T, typename RefWrap, typename... A>
auto invoke(T B::*pmf, RefWrap &&ref, A &&...args) noexcept(
    noexcept((ref.get().*pmf)(std::forward<A>(args)...)))
    -> std::enable_if_t<std::is_function<T>::value &&
                            is_reference_wrapper<std::decay_t<RefWrap>>::value,
                        decltype((ref.get().*pmf)(std::forward<A>(args)...))> {
  return (ref.get().*pmf)(std::forward<A>(args)...);
}

template <typename B, typename T, typename P, typename... A>
auto invoke(T B::*pmf, P &&ptr, A &&...args) noexcept(
    noexcept(((*std::forward<P>(ptr)).*pmf)(std::forward<A>(args)...)))
    -> std::enable_if_t<std::is_function<T>::value &&
                            !is_reference_wrapper<std::decay_t<P>>::value &&
                            !std::is_base_of<B, std::decay_t<P>>::value,
                        decltype(((*std::forward<P>(ptr)).*
                                  pmf)(std::forward<A>(args)...))> {
  return ((*std::forward<P>(ptr)).*pmf)(std::forward<A>(args)...);
}

template <typename B, typename T, typename D>
auto invoke(T B::*pmd, D &&ref) noexcept(noexcept(std::forward<D>(ref).*pmd))
    -> std::enable_if_t<!std::is_function<T>::value &&
                            std::is_base_of<B, std::decay_t<D>>::value,
                        decltype(std::forward<D>(ref).*pmd)> {
  return std::forward<D>(ref).*pmd;
}

template <typename B, typename T, typename RefWrap>
auto invoke(T B::*pmd, RefWrap &&ref) noexcept(noexcept(ref.get().*pmd))
    -> std::enable_if_t<!std::is_function<T>::value &&
                            is_reference_wrapper<std::decay_t<RefWrap>>::value,
                        decltype(ref.get().*pmd)> {
  return ref.get().*pmd;
}

template <typename B, typename T, typename P>
auto invoke(T B::*pmd, P &&ptr) noexcept(noexcept((*std::forward<P>(ptr)).*pmd))
    -> std::enable_if_t<!std::is_function<T>::value &&
                            !is_reference_wrapper<std::decay_t<P>>::value &&
                            !std::is_base_of<B, std::decay_t<P>>::value,
                        decltype((*std::forward<P>(ptr)).*pmd)> {
  return (*std::forward<P>(ptr)).*pmd;
}

template <typename F, class... A>
auto invoke(F &&f, A &&...args) noexcept(
    noexcept(std::forward<F>(f)(std::forward<A>(args)...)))
    -> std::enable_if_t<
        !std::is_member_pointer<std::decay_t<F>>::value,
        decltype(std::forward<F>(f)(std::forward<A>(args)...))> {
  return std::forward<F>(f)(std::forward<A>(args)...);
}

} // namespace detail
} // namespace cxx14_v1
} // namespace portable_concurrency
