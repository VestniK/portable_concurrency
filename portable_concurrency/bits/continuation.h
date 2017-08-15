#pragma once

#include <memory>

namespace portable_concurrency {
inline namespace cxx14_v1 {
namespace detail {

class continuation {
public:
  virtual ~continuation() = default;
  virtual void invoke(const std::shared_ptr<continuation>& self) = 0;
};

} // namespace detail
} // inline namespace cxx14_v1
} // namespace portable_concurrency
