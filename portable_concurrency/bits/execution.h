#pragma once

namespace portable_concurrency {

// Intended to be specialized for user provided executor classes
template<typename T>
struct is_executor {static constexpr bool value = false;};

} // namespace portable_concurrency
