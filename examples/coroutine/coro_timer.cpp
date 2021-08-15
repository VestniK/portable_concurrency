#include "coro_timer.h"

namespace coro {

void timed_queue::push_at(time_point time, timed_queue::coroutine_handle h) {
  std::lock_guard{mutex_}, queue_.emplace(value{time, std::move(h)});
  cv_.notify_one();
}

void timed_queue::stop() {
  std::lock_guard{mutex_}, stop_ = true;
  cv_.notify_one();
}

bool timed_queue::resume_next() {
  std::unique_lock lock{mutex_};
  cv_.wait(lock, [this] { return stop_ || !queue_.empty(); });
  if (stop_)
    return false;
  time_point nearest = queue_.top().time;
  while (cv_.wait_until(
      lock, nearest, [&] { return stop_ || queue_.top().time != nearest; })) {
    if (stop_)
      return false;
    nearest = queue_.top().time;
  }
  coroutine_handle handle = std::move(queue_.top().handle);
  queue_.pop();
  lock.unlock();
  handle.resume();
  return true;
}

timer_executor::~timer_executor() {
  queue_.stop();
  if (worker_.joinable())
    worker_.join();
}

void timer_executor::schedule_timers() {
  while (queue_.resume_next())
    ;
}

timer_executor &timer_executor::instance() {
  static coro::timer_executor executor;
  return executor;
}

} // namespace coro
