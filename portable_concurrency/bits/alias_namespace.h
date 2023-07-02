#pragma once

namespace portable_concurrency {}

#if !defined(PORTABLE_CONCURRENCY_ALIAS_NS)
#define PORTABLE_CONCURRENCY_ALIAS_NS 1
#endif

#if PORTABLE_CONCURRENCY_ALIAS_NS
/**
 * @namespace pc
 * @brief Shortcut for the library public namespace `portable_concurrency`.
 */
namespace pc = portable_concurrency;
#endif

/**
 * @namespace portable_concurrency
 * @brief The library public namespace.
 *
 * This namespace name is long and too verbose. There is an alias namespace @ref
 * pc provided which simplifies this library usage.
 */
