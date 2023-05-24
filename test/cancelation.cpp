#include <future>

#include <gtest/gtest.h>

#include <portable_concurrency/future>

#include "test_tools.h"

namespace portable_concurrency {
namespace test {
namespace {

[[gnu::unused]] void foo(pc::promise<int>, pc::future<std::string>) {}

static_assert(
    std::is_same<detail::promise_arg_t<decltype(foo), future<std::string>>,
                 int>::value,
    "PromiseArgDeduce: should work on function reference");

static_assert(
    std::is_same<detail::promise_arg_t<decltype(&foo), future<std::string>>,
                 int>::value,
    "PromiseArgDeduce: should work on function pointer");

TEST(PromiseArgDeduce, shoud_work_on_lambda) {
  auto lambda = [c = 5u](pc::promise<char> p, pc::future<std::string> f) {
    p.set_value(f.get()[c]);
  };
  static_assert(
      std::is_same<detail::promise_arg_t<decltype(lambda), future<std::string>>,
                   char>::value,
      "PromiseArgDeduce, should work on lambda");
}

TEST(PromiseArgDeduce, shoud_work_on_mutable_lambda) {
  auto lambda = [c = 5u](pc::promise<char> p,
                         pc::future<std::string> f) mutable {
    p.set_value(f.get()[c++]);
  };
  static_assert(
      std::is_same<detail::promise_arg_t<decltype(lambda), future<std::string>>,
                   char>::value,
      "PromiseArgDeduce, should work on mutable lambda");
}

TEST(PromiseCancelation, no_object_held_by_promise_after_future_destruction) {
  auto val = std::make_shared<int>(42);
  std::weak_ptr<int> weak = val;
  auto promise = pc::make_promise<std::shared_ptr<int>>();
  promise.first.set_value(std::move(val));
  promise.second = {};
  EXPECT_FALSE(weak.lock());
}

TEST(PromiseCancelation, set_value_do_nothing_on_canceled_promise) {
  auto promise = pc::make_promise<int>();
  promise.second = {};
  EXPECT_NO_THROW(promise.first.set_value(42));
}

TEST(PromiseCancelation, set_exception_do_nothing_on_canceled_promise) {
  auto promise = pc::make_promise<int>();
  promise.second = {};
  EXPECT_NO_THROW(promise.first.set_exception(
      std::make_exception_ptr(std::runtime_error{"Ooups"})));
}

TEST(PackagedTaskCancelation, function_object_is_destroyed_on_cancel) {
  auto val = std::make_shared<int>(42);
  std::weak_ptr<int> weak = val;
  pc::packaged_task<void()> task{[val = std::move(val)] {}};
  { auto f = task.get_future(); }
  EXPECT_FALSE(weak.lock());
}

TEST(PackagedTaskCancelation, canceled_task_remain_valid) {
  pc::packaged_task<void()> task{[] {}};
  { auto f = task.get_future(); }
  EXPECT_TRUE(task.valid());
}

TEST(PackagedTaskCancelation, run_canceled_task_do_not_throw) {
  pc::packaged_task<void()> task{[] {}};
  { auto f = task.get_future(); }
  EXPECT_NO_THROW(task());
}

TEST(PackagedTaskCancelation,
     run_canceled_task_do_not_execute_stored_function) {
  bool executed = false;
  pc::packaged_task<void()> task{[&executed] { executed = true; }};
  { auto f = task.get_future(); }
  task();
  EXPECT_FALSE(executed);
}

TEST(ContinuationCancelation,
     then_continuation_is_not_executed_afeter_future_destruction) {
  auto promise = pc::make_promise<int>();
  bool executed = false;
  {
    auto f =
        promise.second.then([&executed](pc::future<int>) { executed = true; });
  }
  promise.first.set_value(42);
  EXPECT_FALSE(executed);
}

TEST(ContinuationCancelation,
     then_unwrapped_continuation_is_not_executed_afeter_future_destruction) {
  auto promise = pc::make_promise<int>();
  bool executed = false;
  {
    auto f = promise.second.then([&executed](pc::future<int>) {
      executed = true;
      return pc::make_ready_future();
    });
  }
  promise.first.set_value(42);
  EXPECT_FALSE(executed);
}

TEST(
    ContinuationCancelation,
    then_continuation_with_executor_is_not_executed_afeter_future_destruction) {
  auto promise = pc::make_promise<int>();
  std::atomic<bool> executed{false};
  {
    auto f = promise.second.then(
        g_future_tests_env, [&executed](pc::future<int>) { executed = true; });
  }
  promise.first.set_value(42);
  g_future_tests_env->wait_current_tasks();
  EXPECT_FALSE(executed);
}

TEST(ContinuationCancelation,
     next_continuation_is_not_executed_afeter_future_destruction) {
  auto promise = pc::make_promise<int>();
  bool executed = false;
  {
    auto f = promise.second.next([&executed](int) { executed = true; });
  }
  promise.first.set_value(42);
  EXPECT_FALSE(executed);
}

TEST(
    ContinuationCancelation,
    next_continuation_with_executor_is_not_executed_afeter_future_destruction) {
  auto promise = pc::make_promise<int>();
  std::atomic<bool> executed{false};
  {
    auto f = promise.second.next(g_future_tests_env,
                                 [&executed](int) { executed = true; });
  }
  promise.first.set_value(42);
  g_future_tests_env->wait_current_tasks();
  EXPECT_FALSE(executed);
}

TEST(Promise, is_awaiten_returns_true_before_future_destruction) {
  auto p = pc::make_promise<int>();
  auto f = std::move(p.second);
  EXPECT_TRUE(p.first.is_awaiten());
}

TEST(Promise, is_awaiten_returns_false_after_future_destruction) {
  auto p = pc::make_promise<int &>();
  p.second = {};
  EXPECT_FALSE(p.first.is_awaiten());
}

TEST(Promise, is_awaiten_returns_true_after_future_sharing) {
  auto p = pc::make_promise<int>();
  pc::shared_future<int> sf = std::move(p.second);
  EXPECT_TRUE(p.first.is_awaiten());
}

TEST(Promise, is_awaiten_returns_true_after_attaching_continuation) {
  auto p = pc::make_promise<int>();
  auto f = p.second.next([](int val) { return 5 * val; });
  EXPECT_TRUE(p.first.is_awaiten());
}

TEST(Promise, is_awaiten_returns_false_after_continuation_future_destroyed) {
  auto p = pc::make_promise<int>();
  {
    auto f = p.second.next([](int val) { return 5 * val; });
  }
  EXPECT_FALSE(p.first.is_awaiten());
}

TEST(FutureDetach, invalidates_future) {
  auto p = pc::make_promise<int>();
  p.second.detach();
  EXPECT_FALSE(p.second.valid());
}

TEST(Promise, is_awaiten_returns_true_after_future_detach) {
  auto p = pc::make_promise<std::string>();
  p.second.detach();
  EXPECT_TRUE(p.first.is_awaiten());
}

TEST(FutureDetach, do_not_prevents_tasks_cancelation_added_after_detaching) {
  auto p = pc::make_promise<std::string>();
  bool executed = false;
  {
    auto f = p.second.detach().next([&](std::string) { executed = true; });
  }
  EXPECT_FALSE(executed);
}

TEST(SharedFutureDetach, invalidates_future) {
  auto p = pc::make_promise<int>();
  auto f = p.second.share();
  f.detach();
  EXPECT_FALSE(f.valid());
}

TEST(Promise, is_awaiten_returns_true_after_shared_future_detach) {
  auto p = pc::make_promise<std::string>();
  p.second.share().detach();
  EXPECT_TRUE(p.first.is_awaiten());
}

TEST(SharedFutureDetach,
     do_not_prevents_tasks_cancelation_added_after_detaching) {
  auto p = pc::make_promise<std::string>();
  bool executed = false;
  {
    auto f = p.second.share().detach().next(
        [&](const std::string &) { executed = true; });
  }
  EXPECT_FALSE(executed);
}

TEST(Canceler, is_not_called_by_promise_constructor) {
  size_t call_count = 0u;
  auto promise = pc::make_promise<int>(pc::canceler_arg, [&] { ++call_count; });
  EXPECT_EQ(call_count, 0u);
}

TEST(Canceler, is_called_once_if_future_destroyed_before_set) {
  size_t call_count = 0u;
  {
    auto promise =
        pc::make_promise<int>(pc::canceler_arg, [&] { ++call_count; });
  }
  EXPECT_EQ(call_count, 1u);
}

TEST(Canceler, is_not_called_if_future_destroyed_after_set_value) {
  size_t call_count = 0u;
  {
    auto promise =
        pc::make_promise<int>(pc::canceler_arg, [&] { ++call_count; });
    promise.first.set_value(42);
  }
  EXPECT_EQ(call_count, 0u);
}

TEST(Canceler, is_not_called_if_future_destroyed_after_set_exception) {
  size_t call_count = 0u;
  {
    auto promise =
        pc::make_promise<int &>(pc::canceler_arg, [&] { ++call_count; });
    promise.first.set_exception(
        std::make_exception_ptr(std::runtime_error{"qwe"}));
  }
  EXPECT_EQ(call_count, 0u);
}

TEST(Canceler, is_not_called_if_promise_abandoned) {
  size_t call_count = 0u;
  {
    pc::future<void> f;
    {
      auto promise =
          pc::make_promise<void>(pc::canceler_arg, [&] { ++call_count; });
      f = std::move(promise.second);
    }
  }
  EXPECT_EQ(call_count, 0u);
}

TEST(Canceller, non_const_operation_is_supported) {
  std::shared_ptr<int> resource = std::make_shared<int>(42);
  std::weak_ptr<int> weak = resource;
  auto promise = pc::make_promise<int>(
      pc::canceler_arg,
      [resource = std::move(resource)]() mutable { resource.reset(); });
  promise.second = {};
  EXPECT_TRUE(weak.expired());
}

TEST(Cancelleer, is_destroyed_after_being_called) {
  std::shared_ptr<int> resource = std::make_shared<int>(42);
  std::weak_ptr<int> weak = resource;
  auto promise = pc::make_promise<int>(pc::canceler_arg,
                                       [resource = std::move(resource)] {});
  promise.second = {};
  EXPECT_TRUE(weak.expired());
}

TEST(Canceller, non_copyable_operation_is_supported) {
  bool called = false;
  auto deleter = [&called](int *p) {
    called = true;
    delete p;
  };
  std::unique_ptr<int, decltype(deleter)> resource{new int{42}, deleter};
  auto promise = pc::make_promise<int>(
      pc::canceler_arg,
      [resource = std::move(resource)]() mutable { resource.reset(); });
  promise.second = {};
  EXPECT_TRUE(called);
}

TEST(InterruptableContinuation, broken_promise_delivered_if_valie_is_not_set) {
  auto promise = pc::make_promise<int>();
  pc::future<std::string> future =
      promise.second.then([](pc::promise<std::string>, pc::future<int>) {});
  promise.first.set_value(42);
  EXPECT_FUTURE_ERROR(future.get(), std::future_errc::broken_promise);
}

TEST(InterruptableContinuation,
     future_continuation_detects_if_result_is_not_awaiten) {
  auto promise = pc::make_promise<int>();
  pc::future<std::string &> future;
  bool was_awaiten = true;
  future =
      promise.second.then([&](pc::promise<std::string &> p, pc::future<int>) {
        future = {};
        was_awaiten = p.is_awaiten();
      });
  promise.first.set_value(42);
  EXPECT_EQ(was_awaiten, false);
}

TEST(InterruptableContinuation,
     shared_future_continuation_detects_if_result_is_not_awaiten) {
  auto promise = pc::make_promise<int>();
  pc::future<void> future;
  bool was_awaiten = true;
  future = promise.second.share().then(
      [&](pc::promise<void> p, pc::shared_future<int>) {
        future = {};
        was_awaiten = p.is_awaiten();
      });
  promise.first.set_value(42);
  EXPECT_EQ(was_awaiten, false);
}

} // namespace
} // namespace test
} // namespace portable_concurrency
