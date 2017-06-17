#pragma once

namespace portable_concurrency {
inline namespace cxx14_v1 {
namespace detail {

class continuation {
public:
  virtual ~continuation() = default;
  virtual void invoke() = 0;
};

} // namespace detail
} // inline namespace cxx14_v1
} // namespace portable_concurrency
