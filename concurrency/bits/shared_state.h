#pragma once

#include <cassert>
#include <type_traits>

#include "once_consumable_queue.h"
#include "result_box.h"

namespace experimental {
inline namespace concurrency_v1 {
namespace detail {

class continuation {
public:
  virtual ~continuation() = default;
  virtual void invoke() = 0;
};

using continuations_queue = once_consumable_queue<std::shared_ptr<continuation>>;

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
  virtual void add_continuation(std::shared_ptr<continuation> cnt) = 0;
  virtual void set_continuation(std::shared_ptr<continuation> cnt) = 0;
  virtual std::add_lvalue_reference_t<state_storage_t<T>> value_ref() = 0;
};

template<typename T>
class shared_state: public future_state<T> {
public:
  shared_state() = default;

  shared_state(const shared_state&) = delete;
  shared_state(shared_state&&) = delete;

  template<typename... U>
  void emplace(U&&... u) {
    box_.emplace(std::forward<U>(u)...);
    for (auto& cnt: continuations_.consume())
      cnt->invoke();
  }

  void set_exception(std::exception_ptr error) {
    box_.set_exception(error);
    for (auto& cnt: continuations_.consume())
      cnt->invoke();
  }

  bool is_ready() const override {
    return continuations_.is_consumed();
  }

  void add_continuation(std::shared_ptr<continuation> cnt) override {
    if (!continuations_.push(cnt))
      cnt->invoke();
  }

  void set_continuation(std::shared_ptr<continuation> cnt) override {
    if (!continuations_.replace(cnt))
      cnt->invoke();
  }

  std::add_lvalue_reference_t<state_storage_t<T>> value_ref() override {
    assert(is_ready());
    return box_.get();
  }

private:
  result_box<state_storage_t<T>> box_;
  continuations_queue continuations_;
};

} // namespace detail
} // inline namespace concurrency_v1
} // namespace experimental
