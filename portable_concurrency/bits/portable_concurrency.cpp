#include "latch.h"
#include "result_box.h"
#include "shared_state.h"

namespace portable_concurrency {
inline namespace cxx14_v1 {
namespace detail {

class continuations_stack::waiter final {
public:
  waiter() = default;

  void operator() () {
    std::lock_guard<std::mutex>{mutex_}, notified_ = true;
    cv_.notify_one();
  }

  void wait() {
    std::unique_lock<std::mutex> lock{mutex_};
    cv_.wait(lock, [this]() {return notified_;});
  }

  bool wait_for(std::chrono::nanoseconds timeout) {
    std::unique_lock<std::mutex> lock{mutex_};
    return cv_.wait_for(lock, timeout, [this] {return notified_;});
  }

private:
  std::mutex mutex_;
  std::condition_variable cv_;
  bool notified_ = false;
};

continuations_stack::continuations_stack() = default;
continuations_stack::~continuations_stack() = default;

void continuations_stack::push(value_type cnt) {
  if (!stack_.push(cnt))
    cnt();
}

void continuations_stack::execute() {
  for (auto& cnt: stack_.consume())
    cnt();
}

bool continuations_stack::is_consumed() const {
  return stack_.is_consumed();
}

void continuations_stack::init_waiter() {
  std::call_once(waiter_init_, [this] {
    waiter_ = std::make_unique<waiter>();
    unique_function<void()> cnt = std::ref(*waiter_);
    if (!stack_.push(cnt))
      cnt();
  });
}

void continuations_stack::wait() {
  if (is_consumed())
    return;
  init_waiter();
  waiter_->wait();
}

bool continuations_stack::wait_for(std::chrono::nanoseconds timeout) {
  if (is_consumed())
    return true;
  init_waiter();
  return waiter_->wait_for(timeout);
}

result_box<void>::result_box() noexcept {}

result_box<void>::~result_box() {
  switch (state_) {
    case box_state::empty:
    case box_state::result: break;
    case box_state::exception: error_.~exception_ptr(); break;
  }
}

void result_box<void>::emplace() {
  if (state_ != box_state::empty)
    throw std::future_error(std::future_errc::promise_already_satisfied);
  state_ = box_state::result;
}

void result_box<void>::set_exception(std::exception_ptr error) {
  if (state_ != box_state::empty)
    throw std::future_error(std::future_errc::promise_already_satisfied);
  new(&error_) std::exception_ptr(error);
  state_ = box_state::exception;
}

void result_box<void>::get() {
  assert(state_ != box_state::empty);
  if (state_ == box_state::exception)
    std::rethrow_exception(error_);
}

} // namespace detail

void latch::count_down_and_wait() {
  std::unique_lock<std::mutex> lock{mutex_};
  assert(counter_ > 0);
  if (--counter_ == 0) {
    lock.unlock();
    cv_.notify_all();
    return;
  }
  cv_.wait(lock, [this]() {return counter_ == 0;});
}

void latch::count_down(ptrdiff_t n) {
  {
    std::lock_guard<std::mutex> lock{mutex_};
    assert(counter_ >= n);
    assert(n >= 0);
    counter_ -= n;
    if (counter_ > 0)
      return;
  }
  cv_.notify_all();
}

bool latch::is_ready() const noexcept {
  std::lock_guard<std::mutex> lock{mutex_};
  return counter_ == 0;
}

void latch::wait() const {
  std::unique_lock<std::mutex> lock{mutex_};
  cv_.wait(lock, [this]() {return counter_ == 0;});
}

} // inline namespace cxx14_v1
} // namespace portable_concurrency
