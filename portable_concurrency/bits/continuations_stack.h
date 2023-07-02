#pragma once

#include "once_consumable_stack.h"
#include "small_unique_function.hpp"

namespace portable_concurrency {
inline namespace cxx14_v1 {
namespace detail {

using continuation = small_unique_function<void()>;
extern template struct forward_list_deleter<continuation>;
extern template class once_consumable_stack<continuation>;

class continuations_stack {
public:
  void push(continuation &&cnt);
  template <typename Alloc> void push(continuation &&cnt, const Alloc &alloc) {
    if (!stack_.push(cnt, alloc))
      cnt();
  }

  void execute();
  bool executed() const;

private:
  once_consumable_stack<continuation> stack_;
};

} // namespace detail
} // namespace cxx14_v1
} // namespace portable_concurrency
