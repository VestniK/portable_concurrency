#pragma once

#include <cassert>
#include <exception>
#include <future>

namespace portable_concurrency {
inline namespace cxx14_v1 {
namespace detail {

enum class box_state {
  empty,
  result,
  exception
};

template<typename T>
class result_box {
public:
  result_box() noexcept {}

  result_box(const result_box&) = delete;
  result_box(result_box&&) = delete;

  ~result_box() {
    switch (state_) {
      case box_state::empty: break;
      case box_state::result: value_.~T(); break;
      case box_state::exception: error_.~exception_ptr(); break;
    }
  }

  template<typename... U>
  void emplace(U&&... u) {
    if (state_ != box_state::empty)
      throw std::future_error(std::future_errc::promise_already_satisfied);
    new(&value_) T(std::forward<U>(u)...);
    state_ = box_state::result;
  }

  void set_exception(std::exception_ptr error) {
    if (state_ != box_state::empty)
      throw std::future_error(std::future_errc::promise_already_satisfied);
    new(&error_) std::exception_ptr(error);
    state_ = box_state::exception;
  }

  box_state get_state() const noexcept {return state_;}

  T& get() {
    assert(state_ != box_state::empty);
    if (state_ == box_state::exception)
      std::rethrow_exception(error_);
    return value_;
  }

private:
  box_state state_ = box_state::empty;
  union {
    bool dummy_;
    T value_;
    std::exception_ptr error_;
  };
};

template<>
class result_box<void> {
public:
  result_box() noexcept;

  result_box(const result_box&) = delete;
  result_box(result_box&&) = delete;

  ~result_box();

  void emplace();
  void set_exception(std::exception_ptr error);
  box_state get_state() const noexcept {return state_;}
  void get();

private:
  box_state state_ = box_state::empty;
  union {
    bool dummy_;
    std::exception_ptr error_;
  };
};

} // namespace detail
} // inline namespace cxx14_v1
} // namespace portable_concurrency
