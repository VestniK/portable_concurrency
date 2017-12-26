#pragma once

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
template<typename E>
struct is_executor {
  /**
   * `true` if E is executor type, `false` otherwise
   */
  static constexpr bool value = false;
};

} // namespace portable_concurrency

#ifdef DOXYGEN
/**
 * @headerfile portable_concurrency/execution
 * @ingroup execution
 *
 * Function which must be ADL descoverable for user provided executor classes. This function must schedule execution of
 * the functor passed as second argument on the executor provided with first argument.
 *
 * Functor type meets MoveConstructable, MoveAssignable and Callable (with signature `void()`) standard library
 * requirements.
 *
 * @sa portable_concurrency::is_executor
 */
template<typename Executor, typename Functor>
void post(Executor exec, Functor func);
#endif
