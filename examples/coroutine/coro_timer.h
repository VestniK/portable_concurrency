#pragma once

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <queue>
#include <thread>

#include <experimental/coroutine>

namespace coro {

class timed_queue {
public:
  using coroutine_handle = std::experimental::coroutine_handle<>;
  using clock = std::chrono::steady_clock;
  using time_point = clock::time_point;

  void push_at(time_point time, coroutine_handle h);
  void stop();
  bool resume_next();

private:
  struct value {
    time_point time;
    coroutine_handle handle;

    bool operator<(const value &rhs) const { return time > rhs.time; }
  };

  std::priority_queue<value> queue_;
  std::mutex mutex_;
  std::condition_variable cv_;
  bool stop_ = false;
};

class timer_executor {
public:
  using clock = std::chrono::steady_clock;
  using time_point = clock::time_point;

  ~timer_executor();

  struct timer {
    time_point time;
    timer_executor *executor;

    bool await_ready() const { return time <= clock::now(); }
    void await_resume() {}
    void await_suspend(std::experimental::coroutine_handle<> h) {
      executor->queue_.push_at(time, std::move(h));
    }
  };

  template <typename Rep, typename Period>
  timer sleep(std::chrono::duration<Rep, Period> timeout) {
    return {clock::now() + timeout, this};
  }

  static timer_executor &instance();

private:
  void schedule_timers();

private:
  timed_queue queue_;
  std::thread worker_{&timer_executor::schedule_timers, this};
};

} // namespace coro

template <typename Rep, typename Period>
auto operator co_await(std::chrono::duration<Rep, Period> timeout) {
  return coro::timer_executor::instance().sleep(timeout);
}
