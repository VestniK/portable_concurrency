#include <future>
#include <memory>
#include <string>

#include <gtest/gtest.h>

#include <portable_concurrency/future>

#include "test_helpers.h"
#include "test_tools.h"

namespace portable_concurrency {
namespace {
namespace test {

std::string stringify(pc::shared_future<int> f) { return to_string(f.get()); }

struct SharedFutureThen : future_test {
  pc::promise<int> promise;
  const pc::shared_future<int> future = promise.get_future();
  std::string stringified_value = "42";
};

TEST_F(SharedFutureThen, src_future_remains_valid) {
  auto cnt_future = future.then([](pc::shared_future<int>) {});
  EXPECT_TRUE(future.valid());
}

TEST_F(SharedFutureThen, contunuation_receives_shared_future) {
  auto cnt_future = future.then(
      [](auto&& f) { static_assert(std::is_same<std::decay_t<decltype(f)>, pc::shared_future<int>>::value, ""); });
}

TEST_F(SharedFutureThen, returned_future_is_valid) {
  auto cnt_future = future.then([](pc::shared_future<int>) {});
  EXPECT_TRUE(cnt_future.valid());
}

TEST_F(SharedFutureThen, returned_future_is_not_ready_for_not_ready_source_future) {
  auto cnt_future = future.then([](pc::shared_future<int>) {});
  EXPECT_FALSE(cnt_future.is_ready());
}

TEST_F(SharedFutureThen, returned_future_is_ready_for_ready_source_future) {
  set_promise_value(promise);
  auto cnt_future = future.then([](pc::shared_future<int>) {});
  EXPECT_TRUE(cnt_future.is_ready());
}

TEST_F(SharedFutureThen, returned_future_becomes_ready_when_source_becomes_ready) {
  auto cnt_future = future.then([](pc::shared_future<int>) {});
  set_promise_value(promise);
  EXPECT_TRUE(cnt_future.is_ready());
}

TEST_F(SharedFutureThen, continuation_is_executed_once_for_ready_source) {
  set_promise_value(promise);
  unsigned cnt_exec_count = 0;
  auto cnt_future = future.then([&](pc::shared_future<int>) { ++cnt_exec_count; });
  EXPECT_EQ(cnt_exec_count, 1u);
}

TEST_F(SharedFutureThen, continuation_is_executed_once_when_source_becomes_ready) {
  unsigned cnt_exec_count = 0;
  auto cnt_future = future.then([&](pc::shared_future<int>) { ++cnt_exec_count; });
  promise.set_value(42);
  EXPECT_EQ(cnt_exec_count, 1u);
}

TEST_F(SharedFutureThen, continuation_arg_is_ready_for_ready_source) {
  promise.set_value(42);
  auto cnt_future = future.then([](pc::shared_future<int> f) { return f.is_ready(); });
  EXPECT_TRUE(cnt_future.get());
}

TEST_F(SharedFutureThen, continuation_arg_is_ready_for_not_ready_source) {
  auto cnt_future = future.then([](pc::shared_future<int> f) { return f.is_ready(); });
  promise.set_value(42);
  EXPECT_TRUE(cnt_future.get());
}

TEST_F(SharedFutureThen, exception_from_continuation_delivered_to_returned_future) {
  pc::future<bool> cnt_f =
      future.then([](pc::shared_future<int>) -> bool { throw std::runtime_error("continuation error"); });
  promise.set_value(42);
  EXPECT_RUNTIME_ERROR(cnt_f, "continuation error");
}

TEST_F(SharedFutureThen, value_is_delivered_to_continuation_for_not_ready_source) {
  pc::future<void> cnt_f = future.then([](pc::shared_future<int> f) { EXPECT_EQ(f.get(), 42); });
  promise.set_value(42);
  cnt_f.get();
}

TEST_F(SharedFutureThen, value_is_delivered_to_continuation_for_ready_source) {
  promise.set_value(42);
  pc::future<void> cnt_f = future.then([](pc::shared_future<int> f) { EXPECT_EQ(f.get(), 42); });
  cnt_f.get();
}

TEST_F(SharedFutureThen, result_of_continuation_delivered_to_returned_future) {
  pc::future<std::string> cnt_f = future.then(stringify);
  promise.set_value(42);

  EXPECT_EQ(cnt_f.get(), "42");
}

TEST_F(SharedFutureThen, result_of_continuation_delivered_to_returned_future_for_ready_future) {
  promise.set_value(42);
  pc::future<std::string> cnt_f = future.then(stringify);

  EXPECT_EQ(cnt_f.get(), "42");
}

TEST_F(SharedFutureThen, result_of_continuation_delivered_to_returned_future_for_async_future) {
  pc::shared_future<int> f = set_value_in_other_thread<int>(25ms);

  pc::future<std::string> cnt_f = f.then(stringify);
  EXPECT_EQ(cnt_f.get(), this->stringified_value);
}

TEST_F(SharedFutureThen, exception_to_continuation) {
  pc::shared_future<int> f = set_error_in_other_thread<int>(25ms, std::runtime_error("test error"));

  pc::future<std::string> string_f = f.then([](pc::shared_future<int> ready_f) -> std::string {
    EXPECT_RUNTIME_ERROR(ready_f, "test error");
    return "Exception delivered";
  });

  string_f.get();
}

TEST_F(SharedFutureThen, exception_to_ready_continuation) {
  pc::shared_future<int> f = pc::make_exceptional_future<int>(std::runtime_error("test error"));

  pc::future<std::string> string_f = f.then([](pc::shared_future<int> ready_f) -> std::string {
    EXPECT_RUNTIME_ERROR(ready_f, "test error");
    return "Exception delivered";
  });

  string_f.get();
}

TEST_F(SharedFutureThen, run_continuation_on_specific_executor) {
  pc::future<std::thread::id> cnt_f =
      future.then(g_future_tests_env, [](pc::shared_future<int>) { return std::this_thread::get_id(); });
  set_promise_value(promise);
  EXPECT_TRUE(g_future_tests_env->uses_thread(cnt_f.get()));
}

TEST_F(SharedFutureThen, then_with_executor_supportds_state_abandon) {
  pc::future<std::string> cnt_f = future.then(null_executor, stringify);
  promise.set_value(42);
  EXPECT_FUTURE_ERROR(cnt_f.get(), std::future_errc::broken_promise);
}

TEST_F(SharedFutureThen, all_of_multiple_continuations_are_invoked) {
  bool executed1 = false, executed2 = false;
  pc::future<void> cnt_f1 = future.then([&executed1](pc::shared_future<int>) { executed1 = true; });
  pc::future<void> cnt_f2 = future.then([&executed2](pc::shared_future<int>) { executed2 = true; });
  promise.set_value(123);

  EXPECT_TRUE(executed1);
  EXPECT_TRUE(executed2);
}

} // namespace test
} // anonymous namespace
} // namespace portable_concurrency
