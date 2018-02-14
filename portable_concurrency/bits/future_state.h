#pragma once

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <type_traits>

#include "fwd.h"

#include "unique_function.hpp"
#include "once_consumable_stack.h"
#include "small_unique_function.h"

namespace portable_concurrency {
inline namespace cxx14_v1 {
namespace detail {

using continuation = small_unique_function<void()>;
extern template struct forward_list_deleter<continuation>;
extern template class once_consumable_stack<continuation>;

class waiter final {
public:
  waiter();
  void operator() ();
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

  template<typename Alloc>
  continuations_stack(const Alloc& alloc) {push(std::ref(waiter_), alloc);}


  ~continuations_stack() = default;

  void push(continuation&& cnt);
  template<typename Alloc>
  void push(continuation&& cnt, const Alloc& alloc) {
    if (!stack_.push(cnt, alloc))
      cnt();
  }

  void execute();
  bool executed() const;
  void wait() const;
  bool wait_for(std::chrono::nanoseconds timeout) const;

private:
  once_consumable_stack<continuation> stack_;
  waiter waiter_;
};

struct void_val {};

template<typename T>
using state_storage_t = std::conditional_t<std::is_void<T>::value,
  void_val,
  std::conditional_t<std::is_reference<T>::value,
    std::reference_wrapper<std::remove_reference_t<T>>,
    std::remove_const_t<T>
  >
>;

template<typename T>
class future_state {
public:
  virtual ~future_state() = default;

  virtual void push_continuation(continuation&& cnt) = 0;
  virtual continuations_stack& continuations() = 0;

  // throws stored exception if there is no value. UB if called before continuations are executed.
  virtual std::add_lvalue_reference_t<state_storage_t<T>> value_ref() = 0;
  // returns nullptr if there is no error. UB if called before continuations are executed.
  virtual std::exception_ptr exception() = 0;
};

} // namespace detail
} // inline namespace cxx14_v1
} // namespace portable_concurrency
