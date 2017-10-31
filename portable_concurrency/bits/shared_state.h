#pragma once

#include <cassert>
#include <type_traits>

#include "fwd.h"

#include "future_state.h"
#include "result_box.h"

namespace portable_concurrency {
inline namespace cxx14_v1 {
namespace detail {

template<typename T>
class shared_state final: public future_state<T> {
public:
  shared_state() = default;

  shared_state(const shared_state&) = delete;
  shared_state(shared_state&&) = delete;

  template<typename... U>
  void emplace(U&&... u) {
    box_.emplace(std::forward<U>(u)...);
    continuations_.execute();
  }

  void set_exception(std::exception_ptr error) {
    box_.set_exception(error);
    continuations_.execute();
  }

  continuations_stack& continuations() override {
    return continuations_;
  }

  std::add_lvalue_reference_t<state_storage_t<T>> value_ref() override {
    assert(continuations_.executed());
    return box_.get();
  }

  std::exception_ptr exception() const override {
    assert(continuations_.executed());
    return box_.exception();
  }

private:
  result_box<state_storage_t<T>> box_;
  continuations_stack continuations_;
};

} // namespace detail
} // inline namespace cxx14_v1
} // namespace portable_concurrency
