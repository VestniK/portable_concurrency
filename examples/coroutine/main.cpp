#include <chrono>
#include <iostream>
#include <string_view>

#include <experimental/coroutine>

#include <portable_concurrency/future>

#include "coro_timer.h"

using namespace std::literals;

pc::future<size_t> foo() {
  std::string_view hello = "Hello Coroutine World\n";
  for (char c : hello) {
    co_await 300ms;
    std::cout << c << std::flush;
  }
  co_return hello.size();
}

pc::future<void> bar() { std::cout << co_await foo() << " symbols printed\n"; }

int main() {
  auto res = bar();
  std::this_thread::sleep_for(4s);
  res.get();
  return EXIT_SUCCESS;
}
