#include <functional>

#include "align.h"
#include "closable_queue.hpp"
#include "future.hpp"
#include "future_state.h"
#include "latch.h"
#include "make_future.h"
#include "once_consumable_stack.hpp"
#include "promise.h"
#include "shared_future.hpp"
#include "shared_state.h"
#include "small_unique_function.hpp"
#include "thread_pool.h"
#include "unique_function.hpp"
#include "when_all.h"
#include "when_any.h"

namespace portable_concurrency {
inline namespace cxx14_v1 {
namespace detail {

#if !defined(HAS_STD_ALIGN)

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

[[noreturn]] void throw_bad_func_call() { throw std::bad_function_call{}; }

template class small_unique_function<void()>;
template struct forward_list_deleter<continuation>;
template class once_consumable_stack<continuation>;

waiter::waiter() = default;

void waiter::operator()() {
  std::lock_guard<std::mutex>{mutex_}, notified_ = true;
  cv_.notify_one();
}

void waiter::wait() const {
  std::unique_lock<std::mutex> lock{mutex_};
  cv_.wait(lock, [this] { return notified_; });
}

bool waiter::wait_for(std::chrono::nanoseconds timeout) const {
  std::unique_lock<std::mutex> lock{mutex_};
  return cv_.wait_for(lock, timeout, [this] { return notified_; });
}

void continuations_stack::push(continuation&& cnt) {
  if (!stack_.push(cnt))
    cnt();
}

continuations_stack::continuations_stack() = default;
continuations_stack::~continuations_stack() = default;

void continuations_stack::execute() {
  auto continuations = stack_.consume();
  waiter_();
  for (auto& cnt : continuations)
    cnt();
}

bool continuations_stack::executed() const { return stack_.is_consumed(); }

const waiter& continuations_stack::get_waiter() const { return waiter_; }

template class closable_queue<unique_function<void()>>;

} // namespace detail

namespace {

// P0443R7 states that if task submitted to static_thread_pool exits via exception
// then std::terminate is called. This behavior established by marking this function
// noexcept.
void process_queue(detail::closable_queue<unique_function<void()>>& queue) noexcept {
  unique_function<void()> task;
  while (queue.pop(task))
    task();
}

} // namespace

template class unique_function<void()>;

latch::~latch() {
  std::unique_lock<std::mutex> lock{mutex_};
  cv_.wait(lock, [this] { return waiters_ == 0; });
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
  cv_.wait(lock, [this] { return counter_ == 0; });
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
  cv_.wait(lock, [this] { return counter_ == 0; });
  if (--waiters_ == 0)
    cv_.notify_one();
}

template <>
void future<void>::get() {
  if (!state_)
    throw std::future_error(std::future_errc::no_state);
  wait();
  auto state = std::move(state_);
  state->value_ref();
}

template <>
typename shared_future<void>::get_result_type shared_future<void>::get() const {
  if (!state_)
    throw std::future_error(std::future_errc::no_state);
  wait();
  state_->value_ref();
}

future<void> make_ready_future() {
  promise<void> promise;
  promise.set_value();
  return promise.get_future();
}

future<std::tuple<>> when_all() { return make_ready_future(std::tuple<>{}); }

future<when_any_result<std::tuple<>>> when_any() {
  return make_ready_future(when_any_result<std::tuple<>>{static_cast<std::size_t>(-1), std::tuple<>{}});
}

static_thread_pool::static_thread_pool(std::size_t num_threads) {
  threads_.reserve(num_threads);
  while (num_threads-- > 0)
    threads_.push_back(std::thread{&static_thread_pool::attach, this});
}

static_thread_pool::~static_thread_pool() {
  stop();
  wait();
}

void static_thread_pool::attach() {
  std::lock_guard<std::mutex>{mutex_}, ++attached_threads_;
  process_queue(queue_);
  {
    std::unique_lock<std::mutex> lock{mutex_};
    --attached_threads_;
    cv_.wait(lock, [&] { return attached_threads_ == 0; });
  }
  cv_.notify_all();
}

void static_thread_pool::stop() { queue_.close(); }

void static_thread_pool::wait() {
  queue_.close();
  for (auto& thread : threads_) {
    if (thread.joinable())
      thread.join();
  }
  threads_.clear();
}

void post(static_thread_pool::executor_type exec, unique_function<void()> task) { exec->push(std::move(task)); }

} // namespace cxx14_v1
} // namespace portable_concurrency
