#pragma once

namespace portable_concurrency {

/**
 * @headerfile portable_concurrency/execution
 * @brief Trait which can be specialized for user provided executor types to enable using them as executors.
 *
 * Some functions in the portable_concurrency library allow to specify executor to perform requested actions. Those
 * function are only participate in overload resolution if this trait is specialized for the executor argument and
 * `is_executor<E>::value` is `true`.
 *
 * In addition to this trait specialization there should be ADL discoverable function `post(E, F)` where E is executor
 * type and F is type satisfying @b MoveConstructable, @b MoveAssignable and @b Callable (with signature `void()`)
 * concepts of the standard library. This function should schedule execution if the function specified on the executor
 * provided and exit immediatelly. Function object passed to this function must be called only once.
 */
template<typename T>
struct is_executor {static constexpr bool value = false;};

} // namespace portable_concurrency
