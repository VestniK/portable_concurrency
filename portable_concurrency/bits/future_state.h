#pragma once

#include <chrono>
#include <type_traits>

#include "fwd.h"

#include "continuations_stack.h"

namespace portable_concurrency {
inline namespace cxx14_v1 {
namespace detail {

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
  // Eqivalent to this->continuations().push(cnt, __allocator_type_erased_by(this))
  virtual void push(continuation&& cnt) {this->continuations().push(std::move(cnt));}

  // throws stored exception if there is no value. UB if called before continuations are executed.
  virtual state_storage_t<T>& value_ref() = 0;
  // returns nullptr if there is no error. UB if called before continuations are executed.
  virtual std::exception_ptr exception() = 0;
};

} // namespace detail
} // inline namespace cxx14_v1
} // namespace portable_concurrency
