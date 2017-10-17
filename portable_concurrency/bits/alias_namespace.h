#pragma once

namespace portable_concurrency {}

#if !defined(PORTABLE_CONCURRENCY_ALIAS_NS)
#  define PORTABLE_CONCURRENCY_ALIAS_NS 1
#endif

#if PORTABLE_CONCURRENCY_ALIAS_NS
namespace pc = portable_concurrency;
#endif
