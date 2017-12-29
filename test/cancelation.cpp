#include <gtest/gtest.h>

#include <portable_concurrency/future>

#include "test_tools.h"

namespace portable_concurrency {
namespace test {
namespace {

TEST(PromiseCancelation, no_object_held_by_promise_after_future_destruction) {
  auto val = std::make_shared<int>(42);
  std::weak_ptr<int> weak = val;
  pc::promise<std::shared_ptr<int>> promise;
  promise.set_value(std::move(val));
  promise.get_future();
  EXPECT_FALSE(weak.lock());
}

TEST(PromiseCancelation, set_value_do_nothing_on_canceled_promise) {
  pc::promise<int> promise;
  promise.get_future();
  EXPECT_NO_THROW(promise.set_value(42));
}

TEST(PromiseCancelation, set_exception_do_nothing_on_canceled_promise) {
  pc::promise<int> promise;
  promise.get_future();
  EXPECT_NO_THROW(promise.set_exception(std::make_exception_ptr(std::runtime_error{"Ooups"})));
}

TEST(PackagedTaskCancelation, function_object_is_destroyed_on_cancel) {
  auto val = std::make_shared<int>(42);
  std::weak_ptr<int> weak = val;
  pc::packaged_task<void()> task{[val = std::move(val)] {}};
  task.get_future();
  EXPECT_FALSE(weak.lock());
}

TEST(PackagedTaskCancelation, canceled_task_remain_valid) {
  pc::packaged_task<void()> task{[] {}};
  task.get_future();
  EXPECT_TRUE(task.valid());
}

TEST(PackagedTaskCancelation, run_canceled_task_do_not_throw) {
  pc::packaged_task<void()> task{[] {}};
  task.get_future();
  EXPECT_NO_THROW(task());
}

TEST(PackagedTaskCancelation, run_canceled_task_do_not_execute_stored_function) {
  bool executed = false;
  pc::packaged_task<void()> task{[&executed] {executed = true;}};
  task.get_future();
  task();
  EXPECT_FALSE(executed);
}

TEST(ContinuationCancelation, then_continuation_is_not_executed_afeter_future_destruction) {
  pc::promise<int> promise;
  bool executed = false;
  promise.get_future().then([&executed](pc::future<int>) {executed = true;});
  promise.set_value(42);
  EXPECT_FALSE(executed);
}

TEST(ContinuationCancelation, then_unwrapped_continuation_is_not_executed_afeter_future_destruction) {
  pc::promise<int> promise;
  bool executed = false;
  promise.get_future().then([&executed](pc::future<int>) {executed = true; return pc::make_ready_future();});
  promise.set_value(42);
  EXPECT_FALSE(executed);
}

TEST(ContinuationCancelation, then_continuation_with_executor_is_not_executed_afeter_future_destruction) {
  pc::promise<int> promise;
  std::atomic<bool> executed{false};
  promise.get_future().then(g_future_tests_env, [&executed](pc::future<int>) {executed = true;});
  promise.set_value(42);
  g_future_tests_env->wait_current_tasks();
  EXPECT_FALSE(executed);
}

TEST(ContinuationCancelation, next_continuation_is_not_executed_afeter_future_destruction) {
  pc::promise<int> promise;
  bool executed = false;
  promise.get_future().next([&executed](int) {executed = true;});
  promise.set_value(42);
  EXPECT_FALSE(executed);
}

TEST(ContinuationCancelation, next_continuation_with_executor_is_not_executed_afeter_future_destruction) {
  pc::promise<int> promise;
  std::atomic<bool> executed{false};
  promise.get_future().next(g_future_tests_env, [&executed](int) {executed = true;});
  promise.set_value(42);
  g_future_tests_env->wait_current_tasks();
  EXPECT_FALSE(executed);
}

TEST(Promise, is_awaiten_returns_true_before_future_destruction) {
  pc::promise<std::string> p;
  auto f = p.get_future();
  EXPECT_TRUE(p.is_awaiten());
}

TEST(Promise, is_awaiten_returns_false_after_future_destruction) {
  pc::promise<int&> p;
  p.get_future();
  EXPECT_FALSE(p.is_awaiten());
}

TEST(Promise, is_awaiten_returns_true_after_future_sharing) {
  pc::promise<int> p;
  pc::shared_future<int> sf = p.get_future();
  EXPECT_TRUE(p.is_awaiten());
}

TEST(Promise, is_awaiten_returns_true_after_attaching_continuation) {
  pc::promise<int> p;
  auto f = p.get_future().next([](int val) {return 5*val;});
  EXPECT_TRUE(p.is_awaiten());
}

TEST(Promise, is_awaiten_returns_false_after_continuation_future_abandoned) {
  pc::promise<int> p;
  p.get_future().next([](int val) {return 5*val;});
  EXPECT_FALSE(p.is_awaiten());
}

TEST(FutureDetach, invalidates_future) {
  pc::promise<int> p;
  auto f = p.get_future();
  f.detach();
  EXPECT_FALSE(f.valid());
}

TEST(Promise, is_awaiten_returns_true_after_future_detach) {
  pc::promise<std::string> p;
  p.get_future().detach();
  EXPECT_TRUE(p.is_awaiten());
}

} // namespace
} // namespace test
} // namespace portable_concurrency
