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
  virtual ~basic_shared_state() = default;

  basic_shared_state(const basic_shared_state&) = delete;
  basic_shared_state(basic_shared_state&&) = delete;

  template<typename... U>
  void emplace(U&&... u) {
    if (storage_.state() != either_state::empty)
      throw std::future_error(std::future_errc::promise_already_satisfied);
    storage_.emplace(first_t{}, std::forward<U>(u)...);
    this->execute_continuations();
  }

  void set_exception(std::exception_ptr error) {
    if (storage_.state() != either_state::empty)
      throw std::future_error(std::future_errc::promise_already_satisfied);
    storage_.emplace(second_t{}, error);
    this->execute_continuations();
  }

  std::add_lvalue_reference_t<state_storage_t<T>> value_ref() override {
    assert(this->continuations_executed());
    assert(storage_.state() != either_state::empty);
    if (storage_.state() == either_state::second)
      std::rethrow_exception(storage_.get(second_t{}));
    return storage_.get(first_t{});
  }

  std::exception_ptr exception() override {
    assert(this->continuations_executed());
    assert(storage_.state() != either_state::empty);
    if (storage_.state() == either_state::first)
      return nullptr;
    return storage_.get(second_t{});
  }

private:
  either<state_storage_t<T>, std::exception_ptr> storage_;
};

template<typename T, typename Alloc>
class shared_state final : public basic_shared_state<T> {
  using continuation = typename future_state<T>::continuation;
  using list_node_allocator = typename std::allocator_traits<Alloc>::template rebind_alloc<forward_list_node<continuation>>;
public:
  shared_state() { }
  explicit shared_state(const Alloc& allocator) :
      continuations_(list_node_allocator(allocator))
  { }

  void push_continuation(typename future_state<T>::continuation&& cnt) final {
    continuations_.push(std::move(cnt));
  }
  void execute_continuations() final {
    continuations_.execute();
  }
  bool continuations_executed() const final {
    return continuations_.executed();
  }
  void wait() final {
    continuations_.wait();
  }
  bool wait_for(std::chrono::nanoseconds timeout) final {
    return continuations_.wait_for(timeout);
  }

private:
  continuations_stack<list_node_allocator> continuations_;
};

} // namespace detail
} // inline namespace cxx14_v1
} // namespace portable_concurrency
