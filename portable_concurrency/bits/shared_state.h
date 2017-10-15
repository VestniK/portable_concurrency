#pragma once

#include <cassert>
#include <type_traits>

#include "fwd.h"

#include "once_consumable_stack_fwd.h"
#include "result_box.h"
#include "unique_function.h"

namespace portable_concurrency {
inline namespace cxx14_v1 {
namespace detail {

class continuations_stack  {
public:
  using value_type = unique_function<void()>;

  continuations_stack();
  ~continuations_stack();

  void push(value_type cnt);
  void execute();
  bool is_consumed() const;
  void wait();
  bool wait_for(std::chrono::nanoseconds timeout);

private:
  class waiter;
  void init_waiter();

  once_consumable_stack<value_type> stack_;
  std::once_flag waiter_init_;
  std::unique_ptr<waiter> waiter_;
};

template<typename T>
using state_storage_t = std::conditional_t<
  std::is_reference<T>::value,
  std::reference_wrapper<std::remove_reference_t<T>>,
  std::remove_const_t<T>
>;

template<typename T>
class future_state {
public:
  virtual ~future_state() = default;

  virtual continuations_stack& continuations() = 0;
  virtual std::add_lvalue_reference_t<state_storage_t<T>> value_ref() = 0;
  virtual future_state<future<T>>* as_wrapped() {return nullptr;}
};

template<typename T>
std::shared_ptr<future_state<T>>& state_of(future<T>&);

template<typename T>
std::shared_ptr<future_state<T>>& state_of(shared_future<T>&);

template<typename T>
class shared_state: public future_state<T> {
public:
  shared_state() = default;

  shared_state(const shared_state&) = delete;
  shared_state(shared_state&&) = delete;

  template<typename... U>
  static
  void emplace(const std::shared_ptr<shared_state>& self, U&&... u) {
    self->box_.emplace(std::forward<U>(u)...);
    self->continuations_.execute();
  }

  static
  void set_exception(const std::shared_ptr<shared_state>& self, std::exception_ptr error) {
    self->box_.set_exception(error);
    self->continuations_.execute();
  }

  continuations_stack& continuations() override {
    return continuations_;
  }

  std::add_lvalue_reference_t<state_storage_t<T>> value_ref() override {
    assert(continuations_.is_consumed());
    return box_.get();
  }

private:
  result_box<state_storage_t<T>> box_;
  continuations_stack continuations_;
};

} // namespace detail
} // inline namespace cxx14_v1
} // namespace portable_concurrency
