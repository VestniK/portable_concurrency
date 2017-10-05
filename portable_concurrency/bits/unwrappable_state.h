#pragma once

#include "future.h"

namespace portable_concurrency {
inline namespace cxx14_v1 {
namespace detail {

template<typename Wrap, typename T>
class unwrap_helper: public future_state<T> {
public:
  continuations_stack& continuations() override {
    return static_cast<Wrap*>(this)->unwrap_continuations();
  }

  std::add_lvalue_reference_t<state_storage_t<T>> value_ref() override {
    return static_cast<Wrap*>(this)->unwrap_value_ref();
  }
};

template<typename T>
class future_state<future<T>> {
public:
  virtual ~future_state() = default;

  virtual continuations_stack& continuations() = 0;
  virtual future<T>& value_ref() = 0;
  virtual future_state<T>* as_unwrapped() = 0;
};

template<typename Wrap, typename T>
class wrap_helper: public future_state<future<T>> {
public:
  continuations_stack& continuations() override {
    return static_cast<Wrap*>(this)->wrap_continuations();
  }

  future<T>& value_ref() override {
    return static_cast<Wrap*>(this)->wrap_value_ref();
  }
};

template<typename T>
class shared_state<future<T>> final:
  public wrap_helper<shared_state<future<T>>, T>,
  public unwrap_helper<shared_state<future<T>>, T>
{
public:
  static
  void emplace(const std::shared_ptr<shared_state<future<T>>>& self, future<T>&& val) {
    self->box_.emplace(std::move(val));
    self->continuations_.execute();
    do_unwrap(self);
  }

  static
  void set_exception(const std::shared_ptr<shared_state<future<T>>>& self, std::exception_ptr error) {
    self->box_.set_exception(error);
    self->continuations_.execute();
    self->notify_unwrap();
  }

  // future_state<future<T>> implementation
  continuations_stack& wrap_continuations() {
    return continuations_;
  }

  future<T>& wrap_value_ref() {
    assert(continuations_.is_consumed());
    return box_.get();
  }

  future_state<T>* as_unwrapped() override {return this;}

  // future_state<T> implementation
  continuations_stack& unwrap_continuations() {
    return unwraped_continuations_;
  }

  std::add_lvalue_reference_t<state_storage_t<T>> unwrap_value_ref() {
    assert(unwraped_continuations_.is_consumed());
    auto& inner_future = box_.get();
    if (!inner_future.valid())
      throw std::future_error(std::future_errc::broken_promise);
    return state_of(inner_future)->value_ref();
  }

  future_state<future<T>>* as_wrapped() override {return this;}

private:
  static
  void do_unwrap(const std::shared_ptr<shared_state<future<T>>>& self) {
    assert(self->continuations_.is_consumed());
    if (!self->box_.get().valid()) {
      self->notify_unwrap();
      return;
    }

    detail::state_of(self->box_.get())->continuations().push([self] {self->notify_unwrap();});
  }

  void notify_unwrap() {
    assert(continuations_.is_consumed());
    unwraped_continuations_.execute();
  }

private:
  result_box<future<T>> box_;
  continuations_stack continuations_;
  continuations_stack unwraped_continuations_;
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
