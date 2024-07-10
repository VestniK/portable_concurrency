#include <atomic>
#include <functional>
#include <future>

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

[[noreturn]] void throw_bad_func_call() { throw std::bad_function_call{}; }

template class small_unique_function<void()>;
template struct forward_list_deleter<continuation>;
template class once_consumable_stack<continuation>;

void continuations_stack::push(continuation &&cnt) {
  if (!stack_.push(cnt))
    cnt();
}

void continuations_stack::execute() {
  auto continuations = stack_.consume();
  for (auto &cnt : continuations)
    cnt();
}

bool continuations_stack::executed() const { return stack_.is_consumed(); }

void wait(future_state_base &state) {
  std::mutex mtx;
  std::condition_variable cv;
  bool ready = false;
  state.push([&] {
    std::lock_guard<std::mutex> guard{mtx};
    ready = true;
    cv.notify_one();
  });

  std::unique_lock<std::mutex> lk{mtx};
  cv.wait(lk, [&] { return ready; });
}

template class closable_queue<unique_function<void()>>;

[[noreturn]] void throw_no_state() {
  throw std::future_error{std::future_errc::no_state};
}

[[noreturn]] void throw_already_satisfied() {
  throw std::future_error(std::future_errc::promise_already_satisfied);
}

[[noreturn]] void throw_already_retrieved() {
  throw std::future_error(std::future_errc::future_already_retrieved);
}

std::exception_ptr make_broken_promise() {
  return std::make_exception_ptr(
      std::future_error{std::future_errc::broken_promise});
}

void future_state_base::push(continuation &&cnt) {
  this->continuations().push(std::move(cnt));
}

} // namespace detail

namespace {

// P0443R7 states that if task submitted to static_thread_pool exits via
// exception then std::terminate is called. This behavior is established by
// marking this function noexcept.
void process_queue(
  detail::closable_queue<unique_function<void()>> &queue,
  const std::atomic<bool> &stopped
) noexcept {
  unique_function<void()> task;
  while (!stopped.load(std::memory_order_relaxed) && queue.pop(task))
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

template <> void future<void>::get() {
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
  auto promise_and_future = make_promise<void>();
  promise_and_future.first.set_value();
  return std::move(promise_and_future.second);
}

future<std::tuple<>> when_all() { return make_ready_future(std::tuple<>{}); }

future<when_any_result<std::tuple<>>> when_any() {
  return make_ready_future(when_any_result<std::tuple<>>{
      static_cast<std::size_t>(-1), std::tuple<>{}});
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
  {
    std::lock_guard<std::mutex> lock{mutex_};
    ++attached_threads_;
  }
  process_queue(queue_, stopped_);
  {
    std::unique_lock<std::mutex> lock{mutex_};
    --attached_threads_;
    cv_.wait(lock, [&] { return attached_threads_ == 0; });
  }
  cv_.notify_all();
}

void static_thread_pool::stop() { 
  stopped_.store(true, std::memory_order_relaxed);
  queue_.close();
}

void static_thread_pool::wait() {
  queue_.close();
  for (auto &thread : threads_) {
    if (thread.joinable())
      thread.join();
  }
  threads_.clear();
}

} // namespace cxx14_v1
} // namespace portable_concurrency
