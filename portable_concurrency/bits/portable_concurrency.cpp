#include "latch.h"
#include "make_future.h"
#include "once_consumable_stack.h"
#include "promise.h"
#include "result_box.h"
#include "shared_state.h"
#include "when_all.h"
#include "when_any.h"

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

bool continuations_stack::executed() const {
  return stack_.is_consumed();
}

void continuations_stack::init_waiter() {
  std::call_once(waiter_init_, [this] {
    waiter_ = std::make_unique<waiter>();
    push(std::ref(*waiter_));
  });
}

void continuations_stack::wait() {
  if (executed())
    return;
  init_waiter();
  waiter_->wait();
}

bool continuations_stack::wait_for(std::chrono::nanoseconds timeout) {
  if (executed())
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

future<void> make_ready_future() {
  promise<void> promise;
  promise.set_value();
  return promise.get_future();
}

void promise<void>::set_value() {
  if (!this->state_)
    throw std::future_error(std::future_errc::no_state);
  this->state_->state.emplace();
}

future<std::tuple<>> when_all() {
  return make_ready_future(std::tuple<>{});
}

future<when_any_result<std::tuple<>>> when_any() {
  return make_ready_future(when_any_result<std::tuple<>>{
    static_cast<std::size_t>(-1),
    std::tuple<>{}
  });
}

} // inline namespace cxx14_v1
} // namespace portable_concurrency
