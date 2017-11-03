#include "align.h"
#include "latch.h"
#include "make_future.h"
#include "once_consumable_stack.h"
#include "promise.h"
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

#if defined(__GNUC__ ) && __GNUC__ < 5

// https://stackoverflow.com/a/37679065/128774 with fix of integer overflow
void* align(std::size_t alignment, std::size_t size, void*& ptr, std::size_t& space) {
  std::uintptr_t pn = reinterpret_cast<std::uintptr_t>(ptr);
  std::uintptr_t aligned = (pn + alignment - 1) & -alignment;
  std::size_t padding = aligned - pn;
  if (space < size + padding)
    return nullptr;
  space -= padding;
  return ptr = reinterpret_cast<void*>(aligned);
}

#endif

} // namespace detail

latch::~latch() {
  std::unique_lock<std::mutex> lock{mutex_};
  cv_.wait(lock, [this] {return waiters_ == 0;});
}

void latch::count_down_and_wait() {
  std::unique_lock<std::mutex> lock{mutex_};
  ++waiters_;
  assert(counter_ > 0);
  if (--counter_ == 0) {
    --waiters_;
    cv_.notify_all();
    return;
  }
  cv_.wait(lock, [this] {return counter_ == 0;});
  if (--waiters_ == 0)
    cv_.notify_one();
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
  ++waiters_;
  cv_.wait(lock, [this] {return counter_ == 0;});
  if (--waiters_ == 0)
    cv_.notify_one();
}

future<void> make_ready_future() {
  promise<void> promise;
  promise.set_value();
  return promise.get_future();
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
