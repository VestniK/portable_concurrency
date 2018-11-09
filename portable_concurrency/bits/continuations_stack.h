#pragma once

#include <chrono>
#include <condition_variable>
#include <mutex>

#include "once_consumable_stack.h"
#include "small_unique_function.hpp"

namespace portable_concurrency {
inline namespace cxx14_v1 {
namespace detail {

using continuation = small_unique_function<void()>;
extern template struct forward_list_deleter<continuation>;
extern template class once_consumable_stack<continuation>;

class waiter final {
public:
  waiter();
  void operator()();
  void wait() const;
  bool wait_for(std::chrono::nanoseconds timeout) const;

private:
  mutable std::mutex mutex_;
  mutable std::condition_variable cv_;
  bool notified_ = false;
};

class continuations_stack {
public:
  continuations_stack();
  ~continuations_stack();

  void push(continuation&& cnt);
  template <typename Alloc>
  void push(continuation&& cnt, const Alloc& alloc) {
    if (!stack_.push(cnt, alloc))
      cnt();
  }

  void execute();
  bool executed() const;
  const waiter& get_waiter() const;

private:
  once_consumable_stack<continuation> stack_;
  waiter waiter_;
};

} // namespace detail
} // namespace cxx14_v1
} // namespace portable_concurrency
