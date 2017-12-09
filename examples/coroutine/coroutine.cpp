#include <chrono>
#include <condition_variable>
#include <iostream>
#include <mutex>
#include <optional>
#include <queue>
#include <string_view>
#include <thread>

#include <experimental/coroutine>

#include <portable_concurrency/future>

using namespace std::literals;
using std::chrono::steady_clock;
using time_point = steady_clock::time_point;

template<typename T>
class timed_queue {
  struct timed_value {
    time_point time;
    T value;

    bool operator<(const timed_value& rhs) const {return time > rhs.time;}
  };

  std::priority_queue<timed_value> queue_;
  std::mutex mutex_;
  std::condition_variable cv_;
  bool stop_ = false;
public:
  template<typename... A>
  void push_at(time_point time, A&&... a) {
    std::lock_guard{mutex_}, queue_.emplace(timed_value{time, {std::forward<A>(a)...}});
    cv_.notify_one();
  }

  void stop() {
    std::lock_guard{mutex_}, stop_ = true;
    cv_.notify_one();
  }

  std::optional<T> pop() {
    std::optional<T> res;
    std::unique_lock lock{mutex_};
    cv_.wait(lock, [this]{return stop_ || !queue_.empty();});
    if (stop_)
      return res;
    time_point nearest = queue_.top().time;
    while (cv_.wait_until(lock, nearest, [&]{return stop_ || queue_.top().time != nearest;})) {
      if (stop_)
        return res;
      nearest = queue_.top().time;
    }
    res = std::move(queue_.top().value);
    queue_.pop();
    return res;
  }
};

timed_queue<std::experimental::coroutine_handle<>> g_await_queue;

struct awaitable_time_point {
  time_point time;

  bool await_ready() const {return time <= steady_clock::now();}
  void await_resume() {}
  void await_suspend(std::experimental::coroutine_handle<> h){g_await_queue.push_at(time, std::move(h));}
};

template <class Rep, class Period>
auto operator co_await(std::chrono::duration<Rep, Period> timeout) {
  return awaitable_time_point{steady_clock::now() + timeout};
}

pc::future<size_t> foo() {
  std::string_view hello = "Hello Coroutine World\n";
  for (char c: hello) {
    co_await 300ms;
    std::cout << c << std::flush;
  }
  co_return hello.size();
}

pc::future<void> bar() {
  std::cout << co_await foo() << " symbols printed\n";
}

int main() {
  std::thread timed_await_worker{[]{
    while (auto handle = g_await_queue.pop())
      handle->resume();
  }};

  auto res = bar();
  std::this_thread::sleep_for(4s);
  res.get();

  g_await_queue.stop();
  timed_await_worker.join();
  return EXIT_SUCCESS;
}
