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

template<typename T>
void set_promise_value(concurrency::promise<T>& p) {
  p.set_value(some_value<T>());
}

template<>
void set_promise_value<void>(concurrency::promise<void>& p) {
  p.set_value();
}

template<typename T, typename R, typename P>
auto set_value_in_other_thread(std::chrono::duration<R, P> sleep_duration)
  -> std::enable_if_t<
    !std::is_void<T>::value,
    concurrency::future<T>
  >
{
  concurrency::promise<T> promise;
  auto res = promise.get_future();

  g_future_tests_env->run_async([sleep_duration](
    concurrency::promise<T>& promise
  ) {
    std::this_thread::sleep_for(sleep_duration);
    promise.set_value(some_value<T>());
  }, std::move(promise));

  return res;
}

template<typename T, typename R, typename P>
auto set_value_in_other_thread(std::chrono::duration<R, P> sleep_duration)
  -> std::enable_if_t<
    std::is_void<T>::value,
    concurrency::future<void>
  >
{
  concurrency::promise<void> promise;
  auto res = promise.get_future();

  g_future_tests_env->run_async([sleep_duration](
    concurrency::promise<void>& promise
  ) {
    std::this_thread::sleep_for(sleep_duration);
    promise.set_value();
  }, std::move(promise));

  return res;
}

template<typename T, typename E, typename R, typename P>
auto set_error_in_other_thread(
  std::chrono::duration<R, P> sleep_duration,
  E err
) {
  concurrency::promise<T> promise;
  auto res = promise.get_future();

  g_future_tests_env->run_async([](
    concurrency::promise<T>& promise,
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
