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

struct FutureThen: ::testing::Test {
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

TEST_F(FutureThen, implicitly_unwrapps_futures) {
  pc::promise<void> inner_promise;
  auto cnt_f = future.then([&](pc::future<int>) {
    return inner_promise.get_future();
  });
  static_assert(std::is_same<decltype(cnt_f), pc::future<void>>::value, "");
}

TEST_F(FutureThen, unwrapped_future_is_not_ready_after_continuation_call) {
  pc::promise<void> inner_promise;
  pc::future<void> cnt_f = future.then([&](pc::future<int>) {
    return inner_promise.get_future();
  });
  set_promise_value(promise);
  EXPECT_FALSE(cnt_f.is_ready());
}

TEST_F(FutureThen, unwrapped_future_is_ready_after_continuation_result_becomes_ready) {
  pc::promise<void> inner_promise;
  pc::future<void> cnt_f = future.then([&](pc::future<int>) {
    return inner_promise.get_future();
  });
  set_promise_value(promise);
  inner_promise.set_value();
  EXPECT_TRUE(cnt_f.is_ready());
}

TEST_F(FutureThen, unwrapped_future_carries_broken_promise_for_invalid_result_of_continuation) {
  pc::future<std::string> cnt_f = future.then([](pc::future<int>) {
    return pc::future<std::string>{};
  });
  set_promise_value(promise);
  EXPECT_FUTURE_ERROR(cnt_f.get(), std::future_errc::broken_promise);
}

TEST_F(FutureThen, unwrapped_future_propagates_inner_future_error) {
  pc::promise<void> inner_promise;
  pc::future<void> cnt_f = future.then([&](pc::future<int>) {
    return inner_promise.get_future();
  });
  set_promise_value(promise);
  inner_promise.set_exception(std::make_exception_ptr(std::runtime_error{"Ooups"}));
  EXPECT_RUNTIME_ERROR(cnt_f, "Ooups");
}

TEST_F(FutureThen, exception_from_unwrapped_continuation_propagated_to_returned_future) {
  pc::future<std::unique_ptr<int>> cnt_f = future.then([](pc::future<int>)
    -> pc::future<std::unique_ptr<int>>
  {
    throw std::runtime_error("Ooups");
  });
  set_promise_value(promise);
  EXPECT_RUNTIME_ERROR(cnt_f, "Ooups");
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

TEST_F(FutureThen, next_continuation_receives_value) {
  pc::future<std::string> cnt_f = future.next([](int val) {return std::to_string(val);});
  promise.set_value(42);
  EXPECT_EQ(cnt_f.get(), "42");
}

TEST_F(FutureThen, next_continuation_is_not_called_in_case_of_exception) {
  unsigned call_count = 0;
  pc::future<void> cnt_f = future.next([&call_count](int) {++call_count;});
  promise.set_exception(std::make_exception_ptr(std::runtime_error{"Ooups"}));
  EXPECT_EQ(call_count, 0u);
}

TEST_F(FutureThen, exception_propagated_to_result_of_next) {
  pc::future<void> cnt_f = future.next([](int) {});
  promise.set_exception(std::make_exception_ptr(std::runtime_error{"Ooups"}));
  EXPECT_RUNTIME_ERROR(cnt_f, "Ooups");
}

TEST_F(FutureThen, next_is_executed_for_void_future) {
  pc::promise<void> void_promise;
  pc::future<void> void_future = void_promise.get_future();
  pc::future<int> cnt_f = void_future.next([]() {return 42;});
  void_promise.set_value();
  EXPECT_EQ(cnt_f.get(), 42);
}

TEST_F(FutureThen, next_is_executed_for_ref_future) {
  pc::promise<int&> ref_promise;
  pc::future<int&> ref_future = ref_promise.get_future();
  pc::future<int*> cnt_f = ref_future.next([](int& val) {return &val;});
  int a = 42;
  ref_promise.set_value(a);
  EXPECT_EQ(cnt_f.get(), &a);
}

TEST_F(FutureThen, next_continuation_executed_on_specified_executor) {
  pc::future<std::thread::id> cnt_f = future.next(g_future_tests_env, [](int) {
    return std::this_thread::get_id();
  });
  promise.set_value(42);
  EXPECT_TRUE(g_future_tests_env->uses_thread(cnt_f.get()));
}

} // anonymous namespace
