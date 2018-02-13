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
class basic_shared_state : public future_state<T> {
public:
  basic_shared_state() = default;

  basic_shared_state(const basic_shared_state&) = delete;
  basic_shared_state(basic_shared_state&&) = delete;

  template<typename... U>
  void emplace(U&&... u) {
    if (storage_.state() != either_state::empty)
      throw std::future_error(std::future_errc::promise_already_satisfied);
    storage_.emplace(first_t{}, std::forward<U>(u)...);
    this->continuations().execute();
  }

  void set_exception(std::exception_ptr error) {
    if (storage_.state() != either_state::empty)
      throw std::future_error(std::future_errc::promise_already_satisfied);
    storage_.emplace(second_t{}, error);
    this->continuations().execute();
  }

  std::add_lvalue_reference_t<state_storage_t<T>> value_ref() final {
    assert(this->continuations().executed());
    assert(storage_.state() != either_state::empty);
    if (storage_.state() == either_state::second)
      std::rethrow_exception(storage_.get(second_t{}));
    return storage_.get(first_t{});
  }

  std::exception_ptr exception() final {
    assert(this->continuations().executed());
    assert(storage_.state() != either_state::empty);
    if (storage_.state() == either_state::first)
      return nullptr;
    return storage_.get(second_t{});
  }

  continuations_stack& continuations() final {return continuations_;}

private:
  either<state_storage_t<T>, std::exception_ptr> storage_;
  continuations_stack continuations_;
};

template<typename T, typename Alloc>
class shared_state final : public basic_shared_state<T> {
public:
  shared_state() = default;
  explicit shared_state(const Alloc& allocator):
    alloc_(allocator)
  {}

  void push_continuation(continuation&& cnt) final {
    this->continuations().push(std::move(cnt), alloc_);
  }

private:
  Alloc alloc_;
};

} // namespace detail
} // inline namespace cxx14_v1
} // namespace portable_concurrency
