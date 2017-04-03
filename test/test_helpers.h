#pragma once

#include "test_tools.h"

using namespace std::literals;

// Clang don't want to eat the code bellow:
// template<typename T>
// T some_value() = delete;
// https://llvm.org/bugs/show_bug.cgi?id=17537
// https://llvm.org/bugs/show_bug.cgi?id=18539
template<typename T>
T some_value() {static_assert(sizeof(T) == 0, "some_value<T> is deleted");}

template<>
int some_value<int>() {return 42;}

template<>
std::string some_value<std::string>() {return "hello";}

template<>
std::unique_ptr<int> some_value<std::unique_ptr<int>>() {return std::make_unique<int>(42);}

template<>
future_tests_env& some_value<future_tests_env&>() {return *g_future_tests_env;}

template<>
void some_value<void>() {}

template<typename T>
void set_promise_value(experimental::promise<T>& p) {
  p.set_value(some_value<T>());
}

template<>
void set_promise_value<void>(experimental::promise<void>& p) {
  p.set_value();
}

template<typename T>
experimental::future<T> make_some_ready_future() {
  return experimental::make_ready_future(some_value<T>());
}

template<>
experimental::future<future_tests_env&> make_some_ready_future() {
  return experimental::make_ready_future(std::ref(some_value<future_tests_env&>()));
}

template<>
experimental::future<void> make_some_ready_future() {
  return experimental::make_ready_future();
}

template<typename T, typename R, typename P>
auto set_value_in_other_thread(std::chrono::duration<R, P> sleep_duration) {
  auto task = experimental::packaged_task<T()>{[sleep_duration]() -> T {
    std::this_thread::sleep_for(sleep_duration);
    return some_value<T>();
  }};
  auto res = task.get_future();
  g_future_tests_env->run_async(std::move(task));
  return res;
}

template<typename T, typename E, typename R, typename P>
auto set_error_in_other_thread(
  std::chrono::duration<R, P> sleep_duration,
  E err
) {
  experimental::promise<T> promise;
  auto res = promise.get_future();

  g_future_tests_env->run_async([](
    experimental::promise<T>& promise,
    E worker_err,
    std::chrono::duration<R, P> tm
  ) {
    std::this_thread::sleep_for(tm);
    promise.set_exception(std::make_exception_ptr(worker_err));
  }, std::move(promise), std::move(err), sleep_duration);

  return res;
}

std::string to_string(int val) {
  return std::to_string(val);
}

std::string to_string(const std::unique_ptr<int>& val) {
  return val ? std::to_string(*val) : "nullptr"s;
}

std::string to_string(const future_tests_env& val) {
  return std::to_string(reinterpret_cast<uintptr_t>(&val));
}

std::string to_string(const std::string& val) {
  return val;
}

// EXPECT_SOME_VALUE test that std::future<T> is set with value of some_value<T>() function

template<typename T>
void expect_some_value(experimental::future<T>& f) {
  EXPECT_EQ(some_value<T>(), f.get());
}

inline
void expect_some_value(experimental::future<std::unique_ptr<int>>& f) {
  EXPECT_EQ(*some_value<std::unique_ptr<int>>(), *f.get());
}

inline
void expect_some_value(experimental::future<future_tests_env&>& f) {
  EXPECT_EQ(&some_value<future_tests_env&>(), &f.get());
}

inline
void expect_some_value(experimental::future<void>& f) {
  EXPECT_NO_THROW(f.get());
}

template<typename T>
void expect_some_value(experimental::shared_future<T>& f) {
  EXPECT_EQ(some_value<T>(), f.get());
}

inline
void expect_some_value(experimental::shared_future<std::unique_ptr<int>>& f) {
  EXPECT_EQ(*some_value<std::unique_ptr<int>>(), *f.get());
}

inline
void expect_some_value(experimental::shared_future<future_tests_env&>& f) {
  EXPECT_EQ(&some_value<future_tests_env&>(), &f.get());
}

inline
void expect_some_value(experimental::shared_future<void>& f) {
  EXPECT_NO_THROW(f.get());
}

#define EXPECT_SOME_VALUE(f) expect_some_value(f)
