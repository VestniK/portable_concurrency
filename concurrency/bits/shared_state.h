#pragma once

#include <cassert>

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

template<typename Box>
class shared_state_base {
public:
  shared_state_base() = default;
  virtual ~shared_state_base() = default;

  shared_state_base(const shared_state_base&) = delete;
  shared_state_base(shared_state_base&&) = delete;

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

  bool is_ready() {
    return continuations_.is_consumed();
  }

  void add_continuation(std::shared_ptr<continuation> cnt) {
    if (!continuations_.push(cnt))
      cnt->invoke();
  }

  void set_continuation(std::shared_ptr<continuation> cnt) {
    if (!continuations_.replace(cnt))
      cnt->invoke();
  }

protected:
  Box& box() {return box_;}

private:
  Box box_;
  continuations_queue continuations_;
};

template<typename T>
class shared_state: public shared_state_base<result_box<T>> {
public:
  shared_state() = default;

  T& value_ref() {
    assert(this->is_ready());
    return this->box().get();
  }
};

template<typename T>
class shared_state<T&>: public shared_state_base<result_box<std::reference_wrapper<T>>> {
public:
  shared_state() = default;

  std::reference_wrapper<T> value_ref() {
    assert(this->is_ready());
    return this->box().get();
  }
};

template<>
class shared_state<void>: public shared_state_base<result_box<void>> {
public:
  shared_state() = default;

  void value_ref() {
    assert(this->is_ready());
    this->box().get();
  }
};

} // namespace detail
} // inline namespace concurrency_v1
} // namespace experimental
