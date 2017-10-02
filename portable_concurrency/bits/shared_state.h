#pragma once

#include <cassert>
#include <type_traits>

#include "fwd.h"

#include "once_consumable_stack.h"
#include "result_box.h"
#include "unique_function.h"
#include "wait_continuation.h"

namespace portable_concurrency {
inline namespace cxx14_v1 {
namespace detail {

class continuations_stack  {
public:
  using value_type = unique_function<void()>;

  bool push(value_type& cnt);
  forward_list<value_type> consume();
  bool is_consumed() const;
  wait_continuation& get_waiter();

private:
  once_consumable_stack<value_type> stack_;
  std::once_flag waiter_init_;
  std::unique_ptr<wait_continuation> waiter_;
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

  virtual bool is_ready() const = 0;
  virtual void add_continuation(unique_function<void()> cnt) = 0;
  virtual std::add_lvalue_reference_t<state_storage_t<T>> value_ref() = 0;
  virtual wait_continuation& get_waiter() = 0;
  virtual future_state<future<T>>* as_wrapped() {return nullptr;}
};

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
    for (auto& cnt: self->continuations_.consume())
      cnt();
  }

  static
  void set_exception(const std::shared_ptr<shared_state>& self, std::exception_ptr error) {
    self->box_.set_exception(error);
    for (auto& cnt: self->continuations_.consume())
      cnt();
  }

  bool is_ready() const override {
    return continuations_.is_consumed();
  }

  void add_continuation(unique_function<void()> cnt) override {
    if (!continuations_.push(cnt))
      cnt();
  }

  std::add_lvalue_reference_t<state_storage_t<T>> value_ref() override {
    assert(is_ready());
    return box_.get();
  }

  wait_continuation& get_waiter() override {
    return continuations_.get_waiter();
  }

private:
  result_box<state_storage_t<T>> box_;
  continuations_stack continuations_;
};

extern template class shared_state<void>;

} // namespace detail
} // inline namespace cxx14_v1
} // namespace portable_concurrency
