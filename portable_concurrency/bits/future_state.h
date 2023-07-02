#pragma once

#include <chrono>
#include <type_traits>

#include "fwd.h"

#include "continuations_stack.h"

namespace portable_concurrency {
inline namespace cxx14_v1 {
namespace detail {

struct void_val {};

template <typename T>
using state_storage_t = std::conditional_t<
    std::is_void<T>::value, void_val,
    std::conditional_t<std::is_reference<T>::value,
                       std::reference_wrapper<std::remove_reference_t<T>>,
                       std::remove_const_t<T>>>;

struct future_state_base {
  virtual ~future_state_base() = default;

  virtual continuations_stack &continuations() = 0;
  // May be overloaded by shared_state with custom allocator in order to
  // allocate continuation_stack node properly
  virtual void push(continuation &&cnt);

  // returns nullptr if there is no error. UB if called before continuations are
  // executed.
  virtual std::exception_ptr exception() = 0;
};

void wait(future_state_base &state);

template <typename T> struct future_state : future_state_base {
  // throws stored exception if there is no value. UB if called before
  // continuations are executed.
  virtual state_storage_t<T> &value_ref() = 0;
};

} // namespace detail
} // namespace cxx14_v1
} // namespace portable_concurrency
