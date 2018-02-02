#pragma once

#include <chrono>
#include <mutex>
#include <type_traits>

#include "fwd.h"

#include "unique_function.hpp"
#include "once_consumable_stack_fwd.h"
#include "unique_function.h"
#include "allocate_unique.h"
#include "once_consumable_stack.h"

namespace portable_concurrency {
inline namespace cxx14_v1 {
namespace detail {

template<typename Alloc>
class continuations_stack {
  using allocator_type = typename std::allocator_traits<Alloc>::template rebind_alloc<forward_list_node<unique_function<void()>>>;
public:
  using value_type = unique_function<void()>;

  continuations_stack(const Alloc& allocator = Alloc()) :
      stack_(allocator),
      waiter_(nullptr, unique_allocator_free<waiter_allocator>(allocator))
  { }
  ~continuations_stack() = default;

  void push(value_type cnt);
  void execute();
  bool executed() const;
  void wait();
  bool wait_for(std::chrono::nanoseconds timeout);

private:
  class waiter;
  void init_waiter();

  using waiter_allocator = typename std::allocator_traits<Alloc>::template rebind_alloc<waiter>;

  once_consumable_stack<value_type, allocator_type> stack_;
  std::once_flag waiter_init_;
  std::unique_ptr<waiter, unique_allocator_free<waiter_allocator>> waiter_;
};

template<typename Alloc>
class continuations_stack<Alloc>::waiter final {
public:
  waiter() = default;

  void operator() () {
    std::lock_guard<std::mutex>{mutex_}, notified_ = true;
    cv_.notify_one();
  }

  void wait() {
    std::unique_lock<std::mutex> lock{mutex_};
    cv_.wait(lock, [this]() {return notified_;});
  }

  bool wait_for(std::chrono::nanoseconds timeout) {
    std::unique_lock<std::mutex> lock{mutex_};
    return cv_.wait_for(lock, timeout, [this] {return notified_;});
  }

private:
  std::mutex mutex_;
  std::condition_variable cv_;
  bool notified_ = false;
};

template<typename Alloc>
void continuations_stack<Alloc>::push(value_type cnt) {
  if (!stack_.push(cnt))
    cnt();
}

template<typename Alloc>
void continuations_stack<Alloc>::execute() {
  for (auto& cnt: stack_.consume())
    cnt();
}

template<typename Alloc>
bool continuations_stack<Alloc>::executed() const {
  return stack_.is_consumed();
}

template<typename Alloc>
void continuations_stack<Alloc>::init_waiter() {
  std::call_once(waiter_init_, [this] {
    auto other = allocate_unique<waiter>(waiter_allocator(stack_.get_allocator()));
    waiter_.reset(other.release());
    push(std::ref(*waiter_));
  });
}

template<typename Alloc>
void continuations_stack<Alloc>::wait() {
  if (executed())
    return;
  init_waiter();
  waiter_->wait();
}

template<typename Alloc>
bool continuations_stack<Alloc>::wait_for(std::chrono::nanoseconds timeout) {
  if (executed())
    return true;
  init_waiter();
  return waiter_->wait_for(timeout);
}

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
  using continuation = unique_function<void()>;

  virtual ~future_state() = default;

  virtual void push_continuation(continuation&& cnt) = 0;
  virtual void execute_continuations() = 0;
  virtual bool continuations_executed() const = 0;

  virtual void wait() = 0;
  virtual bool wait_for(std::chrono::nanoseconds timeout) = 0;

  // throws stored exception if there is no value. UB if called before continuations are executed.
  virtual std::add_lvalue_reference_t<state_storage_t<T>> value_ref() = 0;
  // returns nullptr if there is no error.  UB if called before continuations are executed.
  virtual std::exception_ptr exception() = 0;
};

template<typename T>
std::shared_ptr<future_state<T>>& state_of(future<T>&);

template<typename T>
std::shared_ptr<future_state<T>>& state_of(shared_future<T>&);

} // namespace detail
} // inline namespace cxx14_v1
} // namespace portable_concurrency
