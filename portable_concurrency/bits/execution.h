#pragma once

#include <type_traits>
#include <utility>

#if !defined(PC_NO_DEPRECATED)
namespace pc_adl_trampoline {
template <typename E, typename F>
void do_post(E&& e, F&& f) {
  post(std::forward<E>(e), std::forward<F>(f));
}

namespace detail {
template <typename E, typename F>
[[deprecated("Use `post(my_exec, foo)` instedad of `pc::post(my_exec, foo)`")]] void post(E&& e, F&& f) {
  do_post(std::forward<E>(e), std::forward<F>(f));
}

} // namespace detail
} // namespace pc_adl_trampoline
#endif

namespace portable_concurrency {

/**
 * @headerfile portable_concurrency/execution
 * @ingroup execution
 * @brief Trait which can be specialized for user provided types to enable using them as executors.
 *
 * Some functions in the portable_concurrency library allow to specify executor to perform requested actions. Those
 * function are only participate in overload resolution if this trait is specialized for the executor argument and
 * `is_executor<E>::value` is `true`.
 *
 * In addition to this trait specialization there should be @ref post function provided in order to use executor type
 * E with portable_concurrency library.
 *
 * @sa post
 */
template <typename E>
struct is_executor : std::false_type {};

/**
 * @headerfile portable_concurrency/execution
 * @ingroup execution
 * @brief Trivial executor which evaluates task immediattely in the invocation thread
 *
 * This executor is used by `future::then` and other continuation related family of functions by default when no
 * executor is specified explicitly.
 */
class inplace_executor_t {
private:
  template <typename Task>
  friend void post(inplace_executor_t, Task&& task) {
    std::forward<Task>(task)();
  }
};

/**
 * @headerfile portable_concurrency/execution
 * @ingroup execution
 * @brief Global instance of trivial inplace executor type
 *
 * @sa inplace_executor
 */
constexpr inplace_executor_t inplace_executor;

template <>
struct is_executor<inplace_executor_t> : std::true_type {};

#if !defined(PC_NO_DEPRECATED)
using namespace ::pc_adl_trampoline::detail;
#endif

} // namespace portable_concurrency

#ifdef DOXYGEN
/**
 * @headerfile portable_concurrency/execution
 * @ingroup execution
 *
 * Function which must be ADL discoverable for user provided executor classes. This function must schedule execution of
 * the function object from the second argument on the executor provided with the first argument.
 *
 * Function object type meets MoveConstructable, MoveAssignable and Callable (with signature `void()`) standard library
 * named requirements.
 *
 * @sa portable_concurrency::is_executor
 */
template <typename Executor, typename Functor>
void post(Executor exec, Functor func);
#endif
