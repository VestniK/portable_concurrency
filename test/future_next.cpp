#include <string>
#include <memory>

#include <gtest/gtest.h>

#include "portable_concurrency/future"

#include "test_tools.h"
#include "test_helpers.h"

namespace portable_concurrency {
namespace {
namespace test {

struct FutureNext: future_test {
  pc::promise<int> promise;
  pc::future<int> future = promise.get_future();
};

TEST_F(FutureNext, src_future_is_invalidated) {
  pc::future<void> cnt_f = future.next([](int) {});
  EXPECT_FALSE(future.valid());
}

TEST_F(FutureNext, continuation_receives_value) {
  pc::future<std::string> cnt_f = future.next([](int val) {return std::to_string(val);});
  promise.set_value(42);
  EXPECT_EQ(cnt_f.get(), "42");
}

TEST_F(FutureNext, continuation_is_not_called_in_case_of_exception) {
  unsigned call_count = 0;
  pc::future<void> cnt_f = future.next([&call_count](int) {++call_count;});
  promise.set_exception(std::make_exception_ptr(std::runtime_error{"Ooups"}));
  EXPECT_EQ(call_count, 0u);
}

TEST_F(FutureNext, exception_propagated_to_result_of_next) {
  pc::future<void> cnt_f = future.next([](int) {});
  promise.set_exception(std::make_exception_ptr(std::runtime_error{"Ooups"}));
  EXPECT_RUNTIME_ERROR(cnt_f, "Ooups");
}

TEST_F(FutureNext, is_executed_for_void_future) {
  pc::promise<void> void_promise;
  pc::future<void> void_future = void_promise.get_future();
  pc::future<int> cnt_f = void_future.next([]() {return 42;});
  void_promise.set_value();
  EXPECT_EQ(cnt_f.get(), 42);
}

TEST_F(FutureNext, is_executed_for_ref_future) {
  pc::promise<int&> ref_promise;
  pc::future<int&> ref_future = ref_promise.get_future();
  pc::future<int*> cnt_f = ref_future.next([](int& val) {return &val;});
  int a = 42;
  ref_promise.set_value(a);
  EXPECT_EQ(cnt_f.get(), &a);
}

TEST_F(FutureNext, argument_is_moved_to_continuation_function) {
  pc::promise<std::shared_ptr<int>> sp_promise;
  pc::future<std::shared_ptr<int>> sp_future = sp_promise.get_future();
  pc::future<long> cnt_f = sp_future.next([](std::shared_ptr<int> sp) {
    return sp.use_count();
  });
  sp_promise.set_value(std::make_shared<int>(42));
  EXPECT_EQ(cnt_f.get(), 1);
}

TEST_F(FutureNext, continuation_executed_on_specified_executor) {
  pc::future<std::thread::id> cnt_f = future.next(g_future_tests_env, [](int) {
    return std::this_thread::get_id();
  });
  promise.set_value(42);
  EXPECT_TRUE(g_future_tests_env->uses_thread(cnt_f.get()));
}

TEST_F(FutureNext, with_executor_supportds_state_abandon) {
  pc::future<std::string> cnt_f = future.next(null_executor, [](int val) {
    return std::to_string(val);
  });
  promise.set_value(42);
  EXPECT_FUTURE_ERROR(cnt_f.get(), std::future_errc::broken_promise);
}

} // namespace test
} // anonymous namespace
} // namespace portable_concurrency
