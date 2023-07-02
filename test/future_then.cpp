#include <future>
#include <memory>
#include <string>

#include <gtest/gtest.h>

#include "simple_arena_allocator.h"
#include "test_helpers.h"
#include "test_tools.h"
#include <portable_concurrency/future>

namespace portable_concurrency {
namespace {
namespace test {

std::string stringify(pc::future<int> f) { return to_string(f.get()); }

struct FutureThen : future_test {
  pc::promise<int> promise;
  pc::future<int> future = promise.get_future();
  std::string stringified_value = "42";

  static_arena<1024> arena;
  arena_allocator<std::string, static_arena<1024>> allocator{arena};
  pc::promise<std::string> alloc_promise{std::allocator_arg, allocator};
  pc::future<std::string> alloc_future = alloc_promise.get_future();
};

TEST_F(FutureThen, src_future_invalidated) {
  auto cnt_future = future.then([](pc::future<int>) {});
  EXPECT_FALSE(future.valid());
}

TEST_F(FutureThen, contunuation_receives_future) {
  auto cnt_future = future.then([](auto &&f) {
    static_assert(
        std::is_same<std::decay_t<decltype(f)>, pc::future<int>>::value, "");
  });
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

TEST_F(FutureThen,
       returned_future_becomes_ready_when_source_becomes_ready_with_allocator) {
  auto cnt_future = alloc_future.then([](pc::future<std::string>) {});
  set_promise_value(alloc_promise);
  EXPECT_TRUE(cnt_future.is_ready());
}

TEST_F(FutureThen, continuation_is_executed_once_for_ready_source) {
  set_promise_value(promise);
  unsigned cnt_exec_count = 0;
  auto cnt_future = future.then([&](pc::future<int>) { ++cnt_exec_count; });
  EXPECT_EQ(cnt_exec_count, 1u);
}

TEST_F(FutureThen, continuation_is_executed_once_when_source_becomes_ready) {
  unsigned cnt_exec_count = 0;
  auto cnt_future = future.then([&](pc::future<int>) { ++cnt_exec_count; });
  promise.set_value(42);
  EXPECT_EQ(cnt_exec_count, 1u);
}

TEST_F(FutureThen, continuation_arg_is_ready_for_ready_source) {
  promise.set_value(42);
  auto cnt_future = future.then([](pc::future<int> f) { return f.is_ready(); });
  EXPECT_TRUE(cnt_future.get());
}

TEST_F(FutureThen, continuation_arg_is_ready_for_not_ready_source) {
  auto cnt_future = future.then([](pc::future<int> f) { return f.is_ready(); });
  promise.set_value(42);
  EXPECT_TRUE(cnt_future.get());
}

TEST_F(FutureThen, exception_from_continuation_delivered_to_returned_future) {
  pc::future<bool> cnt_f = future.then([](pc::future<int>) -> bool {
    throw std::runtime_error("continuation error");
  });
  promise.set_value(42);
  EXPECT_RUNTIME_ERROR(cnt_f, "continuation error");
}

TEST_F(FutureThen, value_is_delivered_to_continuation_for_not_ready_source) {
  pc::future<void> cnt_f =
      future.then([](pc::future<int> f) { EXPECT_EQ(f.get(), 42); });
  promise.set_value(42);
  cnt_f.get();
}

TEST_F(FutureThen, value_is_delivered_to_continuation_for_ready_source) {
  promise.set_value(42);
  pc::future<void> cnt_f =
      future.then([](pc::future<int> f) { EXPECT_EQ(f.get(), 42); });
  cnt_f.get();
}

TEST_F(FutureThen, result_of_continuation_delivered_to_returned_future) {
  pc::future<std::string> cnt_f = future.then(stringify);
  promise.set_value(42);

  EXPECT_EQ(cnt_f.get(), "42");
}

TEST_F(FutureThen,
       result_of_continuation_delivered_to_returned_future_for_ready_future) {
  promise.set_value(42);
  pc::future<std::string> cnt_f = future.then(stringify);

  EXPECT_EQ(cnt_f.get(), "42");
}

TEST_F(FutureThen,
       result_of_continuation_delivered_to_returned_future_for_async_future) {
  auto f = set_value_in_other_thread<int>(25ms);

  pc::future<std::string> cnt_f = f.then(stringify);
  EXPECT_EQ(cnt_f.get(), this->stringified_value);
}

TEST_F(FutureThen, exception_to_continuation) {
  auto f =
      set_error_in_other_thread<int>(25ms, std::runtime_error("test error"));

  pc::future<std::string> string_f =
      f.then([](pc::future<int> ready_f) -> std::string {
        EXPECT_RUNTIME_ERROR(ready_f, "test error");
        return "Exception delivered";
      });

  string_f.get();
}

TEST_F(FutureThen, exception_to_ready_continuation) {
  auto f = pc::make_exceptional_future<int>(std::runtime_error("test error"));

  pc::future<std::string> string_f =
      f.then([](pc::future<int> ready_f) -> std::string {
        EXPECT_RUNTIME_ERROR(ready_f, "test error");
        return "Exception delivered";
      });

  string_f.get();
}

TEST_F(FutureThen, run_continuation_on_specific_executor) {
  pc::future<std::thread::id> cnt_f =
      future.then(g_future_tests_env,
                  [](pc::future<int>) { return std::this_thread::get_id(); });
  set_promise_value(promise);
  EXPECT_TRUE(g_future_tests_env->uses_thread(cnt_f.get()));
}

TEST_F(FutureThen, desroys_continuation_after_invocation) {
  auto sp = std::make_shared<int>(42);
  std::weak_ptr<int> wp = sp;
  auto cnt_f =
      future.then([sp = std::exchange(sp, nullptr)](pc::future<int> val) {
        return val.get() + *sp;
      });
  promise.set_value(100);
  EXPECT_TRUE(wp.expired());
}

} // namespace test
} // anonymous namespace
} // namespace portable_concurrency
