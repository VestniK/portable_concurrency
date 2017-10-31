#pragma once

#include <cassert>
#include <exception>
#include <future>
#include <type_traits>

#include "fwd.h"

#include "either.h"
#include "future_state.h"

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
    if (storage_.state() != either_state::empty)
      throw std::future_error(std::future_errc::promise_already_satisfied);
    storage_.emplace(first_t{}, std::forward<U>(u)...);
    continuations_.execute();
  }

  void set_exception(std::exception_ptr error) {
    if (storage_.state() != either_state::empty)
      throw std::future_error(std::future_errc::promise_already_satisfied);
    storage_.emplace(second_t{}, error);
    continuations_.execute();
  }

  continuations_stack& continuations() override {
    return continuations_;
  }

  std::add_lvalue_reference_t<state_storage_t<T>> value_ref() override {
    assert(continuations_.executed());
    assert(storage_.state() != either_state::empty);
    if (storage_.state() == either_state::second)
      std::rethrow_exception(storage_.get(second_t{}));
    return storage_.get(first_t{});
  }

  std::exception_ptr exception() override {
    assert(continuations_.executed());
    assert(storage_.state() != either_state::empty);
    if (storage_.state() == either_state::first)
      return nullptr;
    return storage_.get(second_t{});
  }

private:
  either<state_storage_t<T>, std::exception_ptr> storage_;
  continuations_stack continuations_;
};

} // namespace detail
} // inline namespace cxx14_v1
} // namespace portable_concurrency
