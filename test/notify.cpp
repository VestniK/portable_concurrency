#include <gtest/gtest.h>

#include <portable_concurrency/future>
#include <portable_concurrency/latch>

#include "test_tools.h"

namespace portable_concurrency {
namespace {
namespace test {

template <typename Future>
struct future_notify : ::testing::Test {
  pc::promise<int> promise;
  Future future = promise.get_future();
};
using futures = ::testing::Types<pc::future<int>, pc::shared_future<int>>;
TYPED_TEST_CASE(future_notify, futures);

TYPED_TEST(future_notify, is_called_when_future_becomes_ready) {
  bool called = false;
  this->future.notify([&] { called = true; });
  this->promise.set_value(42);
  EXPECT_TRUE(called);
}

TYPED_TEST(future_notify, is_called_when_future_becomes_ready_with_error) {
  bool called = false;
  this->future.notify([&] { called = true; });
  this->promise.set_exception(std::make_exception_ptr(std::runtime_error{"ooups"}));
  EXPECT_TRUE(called);
}

TYPED_TEST(future_notify, is_called_when_attached_to_ready_future) {
  bool called = false;
  this->promise.set_value(100500);
  this->future.notify([&] { called = true; });
  EXPECT_TRUE(called);
}

TYPED_TEST(future_notify, is_called_when_promise_is_broken) {
  bool called = false;
  this->future.notify([&] { called = true; });
  this->promise = {};
  EXPECT_TRUE(called);
}

TYPED_TEST(future_notify, is_called_only_once) {
  unsigned count = 0;
  this->future.notify([&] { ++count; });
  this->promise.set_value(42);
  EXPECT_EQ(count, 1u);
}

TYPED_TEST(future_notify, is_not_called_before_future_becomes_ready) {
  bool called = false;
  this->future.notify([&] { called = true; });
  EXPECT_FALSE(called);
  this->promise.set_value(
      42); // prevent notification call after leaving this test function since it will corrupt the stack
}

TYPED_TEST(future_notify, is_not_called_if_future_destroyed_before_being_set) {
  bool called = false;
  this->future.notify([&] { called = true; });
  this->future = {};
  this->promise.set_value(100500);
  EXPECT_FALSE(called);
}

TYPED_TEST(future_notify, is_called_for_task_running_asyncroniusly) {
  std::atomic<bool> called{false};
  pc::latch latch{2};
  this->future = pc::async(g_future_tests_env, [&latch] {
    latch.count_down_and_wait();
    return 45;
  });
  this->future.notify([&] { called.store(true); });
  latch.count_down();
  g_future_tests_env->wait_current_tasks();
  EXPECT_TRUE(called.load());
}

TYPED_TEST(future_notify, schedules_notification_with_specified_executor) {
  std::thread::id notification_thread;
  this->future.notify(g_future_tests_env, [&] { notification_thread = std::this_thread::get_id(); });
  this->promise.set_value(100500);
  g_future_tests_env->wait_current_tasks();
  EXPECT_TRUE(g_future_tests_env->uses_thread(notification_thread));
}

} // namespace test
} // anonymous namespace
} // namespace portable_concurrency
