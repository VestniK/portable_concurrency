#pragma once

#include <cassert>
#include <type_traits>

#include "fwd.h"

#include "continuation.h"
#include "once_consumable_queue.h"
#include "result_box.h"
#include "wait_continuation.h"

namespace portable_concurrency {
inline namespace cxx14_v1 {
namespace detail {

class continuations_queue  {
public:
  bool push(std::shared_ptr<continuation>& cnt) {return queue_.push(cnt);}
  auto consume() {return queue_.consume();}
  bool is_consumed() const {return queue_.is_consumed();}

  wait_continuation& get_waiter() {
    std::call_once(waiter_init_, [this] {
      waiter_ = std::make_shared<wait_continuation>();
      std::shared_ptr<continuation> wait_cnt = waiter_;
      if (!queue_.push(wait_cnt))
        wait_cnt->invoke();
    });
    return *waiter_;
  }

private:
  once_consumable_queue<std::shared_ptr<continuation>> queue_;
  std::once_flag waiter_init_;
  std::shared_ptr<wait_continuation> waiter_;
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
  virtual void add_continuation(std::shared_ptr<continuation> cnt) = 0;
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

  std::add_lvalue_reference_t<state_storage_t<T>> value_ref() override {
    assert(is_ready());
    return box_.get();
  }

  wait_continuation& get_waiter() override {
    return continuations_.get_waiter();
  }

private:
  result_box<state_storage_t<T>> box_;
  continuations_queue continuations_;
};

} // namespace detail
} // inline namespace cxx14_v1
} // namespace portable_concurrency
