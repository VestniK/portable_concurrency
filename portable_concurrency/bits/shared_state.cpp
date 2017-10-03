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

template class shared_state<void>;

} // namespace detail
} // inline namespace cxx14_v1
} // namespace portable_concurrency
