#pragma once

#include <chrono>
#include <mutex>
#include <type_traits>

#include "fwd.h"

#include "once_consumable_stack_fwd.h"
#include "unique_function.h"

namespace portable_concurrency {
inline namespace cxx14_v1 {
namespace detail {

class continuations_stack {
public:
  using value_type = unique_function<void()>;

  continuations_stack();
  ~continuations_stack();

  void push(value_type cnt);
  void execute();
  bool executed() const;
  void wait();
  bool wait_for(std::chrono::nanoseconds timeout);

private:
  class waiter;
  void init_waiter();

  once_consumable_stack<value_type> stack_;
  std::once_flag waiter_init_;
  std::unique_ptr<waiter> waiter_;
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

  virtual continuations_stack& continuations() = 0;
  // throws stored exception if there is no value
  virtual std::add_lvalue_reference_t<state_storage_t<T>> value_ref() = 0;
  // returns nullptr if there is no error
  virtual std::exception_ptr exception() = 0;
};

template<typename T>
std::shared_ptr<future_state<T>>& state_of(future<T>&);

template<typename T>
std::shared_ptr<future_state<T>>& state_of(shared_future<T>&);

} // namespace detail
} // inline namespace cxx14_v1
} // namespace portable_concurrency
