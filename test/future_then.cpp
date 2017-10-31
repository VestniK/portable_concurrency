#include <string>
#include <memory>

#include <gtest/gtest.h>

#include "portable_concurrency/future"

#include "test_tools.h"
#include "test_helpers.h"

namespace {

using namespace std::literals;

std::string stringify(pc::future<int> f) {
  return std::to_string(f.get());
}

struct FutureThen: future_test {
  pc::promise<int> promise;
  pc::future<int> future = promise.get_future();
  std::string stringified_value = "42";
};

TEST_F(FutureThen, src_future_invalidated) {
  auto cnt_future = future.then([](pc::future<int>) {});
  EXPECT_FALSE(future.valid());
}

TEST_F(FutureThen, returned_future_is_valid) {
  auto cnt_future = future.then([](pc::future<int>) {});
  EXPECT_TRUE(cnt_future.valid());
}

TEST_F(FutureThen, returned_future_is_not_ready_for_not_ready_source_future) {
  auto cnt_future = future.then([](pc::future<int>) {});
  EXPECT_FALSE(cnt_future.is_ready());
}

TEST_F(FutureThen, returned_future_is_ready_for_ready_source_future) {
  set_promise_value(promise);
  auto cnt_future = future.then([](pc::future<int>) {});
  EXPECT_TRUE(cnt_future.is_ready());
}

TEST_F(FutureThen, returned_future_becomes_ready_when_source_becomes_ready) {
  auto cnt_future = future.then([](pc::future<int>) {});
  set_promise_value(promise);
  EXPECT_TRUE(cnt_future.is_ready());
}

TEST_F(FutureThen, continuation_is_executed_once_for_ready_source) {
  set_promise_value(promise);
  unsigned cnt_exec_count = 0;
  auto cnt_future = future.then([&](pc::future<int>) {
    ++cnt_exec_count;
  });
  EXPECT_EQ(cnt_exec_count, 1u);
}

TEST_F(FutureThen, continuation_is_executed_once_when_source_becomes_ready) {
  unsigned cnt_exec_count = 0;
  auto cnt_future = future.then([&](pc::future<int>) {
    ++cnt_exec_count;
  });
  set_promise_value(promise);
  EXPECT_EQ(cnt_exec_count, 1u);
}

TEST_F(FutureThen, continuation_arg_is_ready_for_ready_source) {
  set_promise_value(promise);
  auto cnt_future = future.then([](pc::future<int> f) {
    EXPECT_TRUE(f.is_ready());
  });
}

TEST_F(FutureThen, continuation_arg_is_ready_for_not_ready_source) {
  auto cnt_future = future.then([](pc::future<int> f) {
    EXPECT_TRUE(f.is_ready());
  });
  set_promise_value(promise);
}

TEST_F(FutureThen, exception_from_continuation_delivered_to_returned_future) {
  pc::future<bool> cnt_f = future.then([](pc::future<int>) -> bool {
    throw std::runtime_error("continuation error");
  });
  set_promise_value(promise);
  EXPECT_RUNTIME_ERROR(cnt_f, "continuation error");
}

TEST_F(FutureThen, value_is_delivered_to_continuation_for_not_ready_source) {
  pc::future<void> cnt_f = future.then([](pc::future<int> f) {
    EXPECT_SOME_VALUE(f);
  });
  set_promise_value(promise);
  cnt_f.get();
}

TEST_F(FutureThen, value_is_delivered_to_continuation_for_ready_source) {
  set_promise_value(promise);
  pc::future<void> cnt_f = future.then([](pc::future<int> f) {
    EXPECT_SOME_VALUE(f);
  });
  cnt_f.get();
}

TEST_F(FutureThen, result_of_continuation_delivered_to_returned_future) {
  pc::future<std::string> cnt_f = future.then(stringify);
  set_promise_value(promise);

  EXPECT_EQ(cnt_f.get(), this->stringified_value);
}

TEST_F(FutureThen, ready_continuation_call) {
  set_promise_value(promise);
  pc::future<std::string> cnt_f = future.then(stringify);

  EXPECT_EQ(cnt_f.get(), this->stringified_value);
}

TEST_F(FutureThen, async_continuation_call) {
  auto f = set_value_in_other_thread<int>(25ms);

  pc::future<std::string> cnt_f = f.then(stringify);
  EXPECT_EQ(cnt_f.get(), this->stringified_value);
}

TEST_F(FutureThen, exception_to_continuation) {
  auto f = set_error_in_other_thread<int>(25ms, std::runtime_error("test error"));

  pc::future<std::string> string_f = f.then([](pc::future<int> ready_f) {
    EXPECT_RUNTIME_ERROR(ready_f, "test error");
    return "Exception delivered"s;
  });

  string_f.get();
}

TEST_F(FutureThen, exception_to_ready_continuation) {
  auto f = pc::make_exceptional_future<int>(std::runtime_error("test error"));

  pc::future<std::string> string_f = f.then([](pc::future<int> ready_f) {
    EXPECT_RUNTIME_ERROR(ready_f, "test error");
    return "Exception delivered"s;
  });

  string_f.get();
}

TEST_F(FutureThen, run_continuation_on_specific_executor) {
  pc::future<std::thread::id> cnt_f = future.then(g_future_tests_env, [](pc::future<int>) {
    return std::this_thread::get_id();
  });
  set_promise_value(promise);
  EXPECT_TRUE(g_future_tests_env->uses_thread(cnt_f.get()));
}

TEST_F(FutureThen, run_unwrapped_continuation_on_specific_executor) {
  pc::future<std::thread::id> cnt_f = future.then(g_future_tests_env, [](pc::future<int>) {
    return pc::make_ready_future(std::this_thread::get_id());
  });
  set_promise_value(promise);
  EXPECT_TRUE(g_future_tests_env->uses_thread(cnt_f.get()));
}

TEST_F(FutureThen, then_with_executor_supportds_state_abandon) {
  pc::future<std::string> cnt_f = future.then(null_executor, [](pc::future<int> f) {
    return std::to_string(f.get());
  });
  promise.set_value(42);
  EXPECT_FUTURE_ERROR(cnt_f.get(), std::future_errc::broken_promise);
}

} // anonymous namespace
