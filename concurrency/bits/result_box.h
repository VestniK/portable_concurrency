#pragma once

#include <exception>

namespace concurrency {

namespace detail {

enum class box_state {
    empty,
    result,
    exception
};

template<typename T>
class result_box {
public:
  result_box() {}

  result_box(const result_box&) = delete;
  result_box(result_box&&) = delete;

  ~result_box() {
    switch (state_) {
      case box_state::empty: break;
      case box_state::result: value_.~T(); break;
      case box_state::exception: error_.~exception_ptr(); break;
    }
  }

  void set_value(const T& val) {
    enshure_setable();
    new(&value_) T(val);
    state_ = box_state::result;
  }

  void set_value(T&& val) {
    enshure_setable();
    new(&value_) T(std::move(val));
    state_ = box_state::result;
  }

  void set_exception(std::exception_ptr error) {
    enshure_setable();
    new(&error_) std::exception_ptr(error);
    state_ = box_state::exception;
  }

  box_state get_state() const noexcept {return state_;}

  T get() {
    assert(state_ != box_state::empty);
    if (retrieved)
      throw future_error(future_errc::future_already_retrieved);
    retrieved = true;
    if (state_ == box_state::exception)
      std::rethrow_exception(error_);
    return std::move(value_);
  }

private:
  void enshure_setable() {
    if (state_ != box_state::empty)
      throw future_error(future_errc::promise_already_satisfied);
    if (retrieved)
      throw future_error(future_errc::future_already_retrieved);
  }

private:
  box_state state_ = box_state::empty;
  bool retrieved = false;
  union {
    bool dummy_;
    T value_;
    std::exception_ptr error_;
  };
};

} // namespace detail

} // namespace concurrency
