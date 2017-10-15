#include <string>
#include <memory>

#include <gtest/gtest.h>

#include "portable_concurrency/future"

#include "test_tools.h"
#include "test_helpers.h"

namespace {

using namespace std::literals;

template<typename T>
std::string stringify(pc::future<T> f) {
  return to_string(f.get());
}

template<>
std::string stringify<void>(pc::future<void> f) {
  f.get();
  return "void value";
}

template<typename T>
struct FutureThenCommon: ::testing::Test {
  pc::promise<T> promise;
  pc::future<T> future = promise.get_future();
};

template<typename T>
struct FutureThen: FutureThenCommon<T> {
  std::string stringified_value = to_string(some_value<T>());
};

template<>
struct FutureThen<void>: FutureThenCommon<void> {
  std::string stringified_value = "void value";
};
TYPED_TEST_CASE(FutureThen, TestTypes);

TYPED_TEST(FutureThen, src_future_invalidated) {
  auto cnt_future = this->future.then([](pc::future<TypeParam>) {});
  EXPECT_FALSE(this->future.valid());
}

TYPED_TEST(FutureThen, returned_future_is_valid) {
  auto cnt_future = this->future.then([](pc::future<TypeParam>) {});
  EXPECT_TRUE(cnt_future.valid());
}

TYPED_TEST(FutureThen, returned_future_is_not_ready_for_not_ready_source_future) {
  auto cnt_future = this->future.then([](pc::future<TypeParam>) {});
  EXPECT_FALSE(cnt_future.is_ready());
}

TYPED_TEST(FutureThen, returned_future_is_ready_for_ready_source_future) {
  set_promise_value(this->promise);
  auto cnt_future = this->future.then([](pc::future<TypeParam>) {});
  EXPECT_TRUE(cnt_future.is_ready());
}

TYPED_TEST(FutureThen, returned_future_becomes_ready_when_source_becomes_ready) {
  auto cnt_future = this->future.then([](pc::future<TypeParam>) {});
  set_promise_value(this->promise);
  EXPECT_TRUE(cnt_future.is_ready());
}

TYPED_TEST(FutureThen, continuation_is_executed_once_for_ready_source) {
  set_promise_value(this->promise);
  unsigned cnt_exec_count = 0;
  auto cnt_future = this->future.then([&](pc::future<TypeParam>) {
    ++cnt_exec_count;
  });
  EXPECT_EQ(cnt_exec_count, 1u);
}

TYPED_TEST(FutureThen, continuation_is_executed_once_when_source_becomes_ready) {
  unsigned cnt_exec_count = 0;
  auto cnt_future = this->future.then([&](pc::future<TypeParam>) {
    ++cnt_exec_count;
  });
  set_promise_value(this->promise);
  EXPECT_EQ(cnt_exec_count, 1u);
}

TYPED_TEST(FutureThen, continuation_arg_is_ready_for_ready_source) {
  set_promise_value(this->promise);
  auto cnt_future = this->future.then([](pc::future<TypeParam> f) {
    EXPECT_TRUE(f.is_ready());
  });
}

TYPED_TEST(FutureThen, continuation_arg_is_ready_for_not_ready_source) {
  auto cnt_future = this->future.then([](pc::future<TypeParam> f) {
    EXPECT_TRUE(f.is_ready());
  });
  set_promise_value(this->promise);
}

TYPED_TEST(FutureThen, exception_from_continuation_delivered_to_returned_future) {
  pc::future<TypeParam> cnt_f = this->future.then([](pc::future<TypeParam>) -> TypeParam {
    throw std::runtime_error("continuation error");
  });
  set_promise_value(this->promise);
  EXPECT_RUNTIME_ERROR(cnt_f, "continuation error");
}

TYPED_TEST(FutureThen, value_is_delivered_to_continuation_for_not_ready_source) {
  pc::future<void> cnt_f = this->future.then([](pc::future<TypeParam> f) {
    EXPECT_SOME_VALUE(f);
  });
  set_promise_value(this->promise);
  cnt_f.get();
}

TYPED_TEST(FutureThen, value_is_delivered_to_continuation_for_ready_source) {
  set_promise_value(this->promise);
  pc::future<void> cnt_f = this->future.then([](pc::future<TypeParam> f) {
    EXPECT_SOME_VALUE(f);
  });
  cnt_f.get();
}

TYPED_TEST(FutureThen, result_of_continuation_delivered_to_returned_future) {
  pc::future<std::string> cnt_f = this->future.then(stringify<TypeParam>);
  set_promise_value(this->promise);

  EXPECT_EQ(cnt_f.get(), this->stringified_value);
}

TYPED_TEST(FutureThen, ready_continuation_call) {
  set_promise_value(this->promise);
  pc::future<std::string> cnt_f = this->future.then(stringify<TypeParam>);

  EXPECT_EQ(cnt_f.get(), this->stringified_value);
}

TYPED_TEST(FutureThen, async_continuation_call) {
  auto f = set_value_in_other_thread<TypeParam>(25ms);

  pc::future<std::string> cnt_f = f.then(stringify<TypeParam>);
  EXPECT_EQ(cnt_f.get(), this->stringified_value);
}

TYPED_TEST(FutureThen, exception_to_continuation) {
  auto f = set_error_in_other_thread<TypeParam>(25ms, std::runtime_error("test error"));

  pc::future<std::string> string_f = f.then([](pc::future<TypeParam> ready_f) {
    EXPECT_RUNTIME_ERROR(ready_f, "test error");
    return "Exception delivered"s;
  });

  string_f.get();
}

TYPED_TEST(FutureThen, exception_to_ready_continuation) {
  auto f = pc::make_exceptional_future<TypeParam>(std::runtime_error("test error"));

  pc::future<std::string> string_f = f.then([](pc::future<TypeParam> ready_f) {
    EXPECT_RUNTIME_ERROR(ready_f, "test error");
    return "Exception delivered"s;
  });

  string_f.get();
}

TYPED_TEST(FutureThen, implicitly_unwrapps_futures) {
  pc::promise<void> inner_promise;
  auto cnt_f = this->future.then([&](pc::future<TypeParam>) {
    return inner_promise.get_future();
  });
  static_assert(std::is_same<decltype(cnt_f), pc::future<void>>::value, "");
}

TYPED_TEST(FutureThen, unwrapped_future_is_not_ready_after_continuation_call) {
  pc::promise<void> inner_promise;
  pc::future<void> cnt_f = this->future.then([&](pc::future<TypeParam>) {
    return inner_promise.get_future();
  });
  set_promise_value(this->promise);
  EXPECT_FALSE(cnt_f.is_ready());
}

TYPED_TEST(FutureThen, unwrapped_future_is_ready_after_continuation_result_becomes_ready) {
  pc::promise<void> inner_promise;
  pc::future<void> cnt_f = this->future.then([&](pc::future<TypeParam>) {
    return inner_promise.get_future();
  });
  set_promise_value(this->promise);
  inner_promise.set_value();
  EXPECT_TRUE(cnt_f.is_ready());
}

} // anonymous namespace
