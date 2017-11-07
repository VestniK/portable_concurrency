#include <string>
#include <memory>

#include <gtest/gtest.h>

#include <portable_concurrency/future>

#include "test_tools.h"
#include "test_helpers.h"

namespace portable_concurrency {
namespace {
namespace test {

struct SharedFutureNext: future_test {
  pc::promise<int> promise;
  pc::shared_future<int> future = promise.get_future();
};

TEST_F(SharedFutureNext, src_future_remains_valid) {
  pc::future<void> cnt_f = future.next([](int) {});
  EXPECT_TRUE(future.valid());
}

TEST_F(SharedFutureNext, continuation_gets_result_via_lvalue_reference) {
  pc::future<void> cnt_f = future.next([](auto&& arg) {
    static_assert(std::is_same<std::decay_t<decltype(arg)>, int>::value, "");
    static_assert(std::is_lvalue_reference<decltype(arg)>::value, "");
  });
}

TEST_F(SharedFutureNext, continuation_receives_value) {
  pc::future<std::string> cnt_f = future.next([](int val) {return std::to_string(val);});
  promise.set_value(42);
  EXPECT_EQ(cnt_f.get(), "42");
}

TEST_F(SharedFutureNext, continuation_is_not_called_in_case_of_exception) {
  unsigned call_count = 0;
  pc::future<void> cnt_f = future.next([&call_count](int) {++call_count;});
  promise.set_exception(std::make_exception_ptr(std::runtime_error{"Ooups"}));
  EXPECT_EQ(call_count, 0u);
}

TEST_F(SharedFutureNext, exception_propagated_to_result_of_next) {
  pc::future<void> cnt_f = future.next([](int) {});
  promise.set_exception(std::make_exception_ptr(std::runtime_error{"Ooups"}));
  EXPECT_RUNTIME_ERROR(cnt_f, "Ooups");
}

TEST_F(SharedFutureNext, is_executed_for_void_future) {
  pc::promise<void> void_promise;
  pc::future<void> void_future = void_promise.get_future();
  pc::future<int> cnt_f = void_future.next([]() {return 42;});
  void_promise.set_value();
  EXPECT_EQ(cnt_f.get(), 42);
}

TEST_F(SharedFutureNext, is_executed_for_ref_future) {
  pc::promise<int&> ref_promise;
  pc::future<int&> ref_future = ref_promise.get_future();
  pc::future<int*> cnt_f = ref_future.next([](int& val) {return &val;});
  int a = 42;
  ref_promise.set_value(a);
  EXPECT_EQ(cnt_f.get(), &a);
}

TEST_F(SharedFutureNext, continuation_executed_on_specified_executor) {
  pc::future<std::thread::id> cnt_f = future.next(g_future_tests_env, [](int) {
    return std::this_thread::get_id();
  });
  promise.set_value(42);
  EXPECT_TRUE(g_future_tests_env->uses_thread(cnt_f.get()));
}

TEST_F(SharedFutureNext, executor_version_supports_state_abandon) {
  pc::future<std::string> cnt_f = future.next(null_executor, [](int val) {
    return std::to_string(val);
  });
  promise.set_value(42);
  EXPECT_FUTURE_ERROR(cnt_f.get(), std::future_errc::broken_promise);
}

TEST_F(SharedFutureNext, multiple_continuations) {
  pc::future<int> cnt_f1 = future.next([](int arg) {return 2*arg;});
  pc::future<int> cnt_f2 = future.next([](int arg) {return 3*arg;});
  promise.set_value(100);
  EXPECT_EQ(cnt_f1.get(), 200);
  EXPECT_EQ(cnt_f2.get(), 300);
}

TEST_F(SharedFutureNext, upwraps_future) {
  pc::promise<std::string> inner_promise;
  pc::future<std::string> cnt_f = future.next([&inner_promise](int) {return inner_promise.get_future();});
  promise.set_value(100500);
  inner_promise.set_value("qwe");
  EXPECT_EQ(cnt_f.get(), "qwe");
}

TEST_F(SharedFutureNext, upwraps_shared_future) {
  auto cnt_f = future.next([](int) {
    return pc::make_ready_future(100500).share();
  });
  static_assert(std::is_same<decltype(cnt_f), pc::shared_future<int>>::value, "");
}

} // namespace test
} // anonymous namespace
} // namespace portable_concurrency
