#include <gtest/gtest.h>

#include <portable_concurrency/future>
#include <portable_concurrency/latch>

#include "test_tools.h"

namespace portable_concurrency {
namespace {
namespace test {

struct future_notify : ::testing::Test {
  pc::promise<int> promise;
  pc::future<int> future = promise.get_future();
};

TEST_F(future_notify, is_called_when_future_becomes_ready) {
  bool called = false;
  future.notify([&] { called = true; });
  promise.set_value(42);
  EXPECT_TRUE(called);
}

TEST_F(future_notify, is_called_when_future_becomes_ready_with_error) {
  bool called = false;
  future.notify([&] { called = true; });
  promise.set_exception(std::make_exception_ptr(std::runtime_error{"ooups"}));
  EXPECT_TRUE(called);
}

TEST_F(future_notify, is_called_when_attached_to_ready_future) {
  bool called = false;
  promise.set_value(100500);
  future.notify([&] { called = true; });
  EXPECT_TRUE(called);
}

TEST_F(future_notify, is_called_when_promise_is_broken) {
  bool called = false;
  future.notify([&] { called = true; });
  promise = {};
  EXPECT_TRUE(called);
}

TEST_F(future_notify, is_called_only_once) {
  unsigned count = 0;
  future.notify([&] { ++count; });
  promise.set_value(42);
  EXPECT_EQ(count, 1u);
}

TEST_F(future_notify, is_not_called_before_future_becomes_ready) {
  bool called = false;
  future.notify([&] { called = true; });
  EXPECT_FALSE(called);
  promise.set_value(42); // prevent notification call after leaving this test function since it will corrupt the stack
}

TEST_F(future_notify, is_not_called_if_future_destroyed_before_being_set) {
  bool called = false;
  future.notify([&] { called = true; });
  future = {};
  promise.set_value(100500);
  EXPECT_FALSE(called);
}

TEST_F(future_notify, is_called_for_task_running_asyncroniusly) {
  std::atomic<bool> called{false};
  pc::latch latch{2};
  future = pc::async(g_future_tests_env, [&latch] {
    latch.count_down_and_wait();
    return 45;
  });
  future.notify([&] { called.store(true); });
  latch.count_down();
  g_future_tests_env->wait_current_tasks();
  EXPECT_TRUE(called.load());
}

} // namespace test
} // anonymous namespace
} // namespace portable_concurrency
