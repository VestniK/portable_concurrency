#pragma once

#include "future.h"
#include "wait_continuation.h"

namespace portable_concurrency {
inline namespace cxx14_v1 {
namespace detail {

template<typename Wrap, typename T>
class unwrap_helper: public future_state<T> {
public:
  bool is_ready() const override {
    return static_cast<const Wrap*>(this)->unwrap_is_ready();
  }

  void add_continuation(std::shared_ptr<continuation> cnt) override {
    return static_cast<Wrap*>(this)->unwrap_add_continuation(cnt);
  }

  std::add_lvalue_reference_t<state_storage_t<T>> value_ref() override {
    return static_cast<Wrap*>(this)->unwrap_value_ref();
  }

  wait_continuation& get_waiter() override {
    return static_cast<Wrap*>(this)->unwrap_get_waiter();
  }

  future_state<future<T>>* as_wrapped() override {
    return static_cast<Wrap*>(this)->unwrap_as_wrapped();
  }
};

template<typename T>
class future_state<future<T>> {
public:
  virtual ~future_state() = default;

  virtual bool is_ready() const = 0;
  virtual void add_continuation(std::shared_ptr<continuation> cnt) = 0;
  virtual future<T>& value_ref() = 0;
  virtual wait_continuation& get_waiter() = 0;
  virtual future_state<T>* as_unwrapped() = 0;
};

template<typename Wrap, typename T>
class wrap_helper: public future_state<future<T>> {
public:
  bool is_ready() const override {
    return static_cast<const Wrap*>(this)->wrap_is_ready();
  }

  void add_continuation(std::shared_ptr<continuation> cnt) override {
    return static_cast<Wrap*>(this)->wrap_add_continuation(cnt);
  }

  future<T>& value_ref() override {
    return static_cast<Wrap*>(this)->wrap_value_ref();
  }

  wait_continuation& get_waiter() override {
    return static_cast<Wrap*>(this)->wrap_get_waiter();
  }
};

template<typename T>
class shared_state<future<T>>:
  public wrap_helper<shared_state<future<T>>, T>,
  public unwrap_helper<shared_state<future<T>>, T>,
  public continuation,
  public std::enable_shared_from_this<continuation>
{
public:
  void emplace(future<T>&& val) {
    box_.emplace(std::move(val));
    for (auto& cnt: continuations_.consume())
      cnt->invoke();
    do_unwrap();
  }

  void set_exception(std::exception_ptr error) {
    box_.set_exception(error);
    for (auto& cnt: continuations_.consume())
      cnt->invoke();
    invoke();
  }

  // future_state<future<T>> implementation
  bool wrap_is_ready() const {
    return continuations_.is_consumed();
  }

  void wrap_add_continuation(std::shared_ptr<continuation> cnt) {
    if (!continuations_.push(cnt))
      cnt->invoke();
  }

  future<T>& wrap_value_ref() {
    assert(wrap_is_ready());
    return box_.get();
  }

  wait_continuation& wrap_get_waiter() {
    return continuations_.get_waiter();
  }

  future_state<T>* as_unwrapped() override {return this;}

  // continuation implementation
  void invoke() override {
    assert(wrap_is_ready());
    for (auto& cnt: unwraped_continuations_.consume())
      cnt->invoke();
  }

  // future_state<T> implementation
  bool unwrap_is_ready() const {
    return unwraped_continuations_.is_consumed();
  }

  void unwrap_add_continuation(std::shared_ptr<continuation> cnt) {
    if (!unwraped_continuations_.push(cnt))
      cnt->invoke();
  }

  std::add_lvalue_reference_t<state_storage_t<T>> unwrap_value_ref() {
    assert(unwrap_is_ready());
    auto& inner_future = box_.get();
    if (!inner_future.valid())
      throw std::future_error(std::future_errc::broken_promise);
    return state_of(inner_future)->value_ref();
  }

  wait_continuation& unwrap_get_waiter() {
    return unwraped_continuations_.get_waiter();
  }

  future_state<future<T>>* unwrap_as_wrapped() {return this;}

private:
  void do_unwrap() {
    assert(wrap_is_ready());
    if (!box_.get().valid()) {
      invoke();
      return;
    }

    detail::state_of(box_.get())->add_continuation(this->shared_from_this());
  }

private:
  result_box<future<T>> box_;
  continuations_queue continuations_;

  continuations_queue unwraped_continuations_;
};

} // namespace detail

template<typename T>
future<T>::future(future<future<T>>&& wrapped) noexcept:
  state_(detail::state_of(wrapped), detail::state_of(wrapped)->as_unwrapped())
{
  wrapped = {};
}

} // inline namespace cxx14_v1
} // namespace portable_concurrency
