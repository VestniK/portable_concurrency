#include <string>
#include <memory>

#include <gtest/gtest.h>

#include "portable_concurrency/future"

#include "test_tools.h"
#include "test_helpers.h"

namespace {

using namespace std::literals;

namespace tests {

template<typename T>
void continuation_call() {
  pc::promise<T> p;
  auto f = p.get_future();
  ASSERT_TRUE(f.valid());

  pc::future<std::string> string_f = f.then([](pc::future<T>&& ready_f) {
    EXPECT_TRUE(ready_f.is_ready());
    return to_string(ready_f.get());
  });
  EXPECT_FALSE(f.valid());
  EXPECT_TRUE(string_f.valid());
  EXPECT_FALSE(string_f.is_ready());

  p.set_value(some_value<T>());
  EXPECT_TRUE(string_f.is_ready());
  EXPECT_EQ(string_f.get(), to_string(some_value<T>()));
}

template<>
void continuation_call<void>() {
  pc::promise<void> p;
  auto f = p.get_future();
  ASSERT_TRUE(f.valid());

  pc::future<std::string> string_f = f.then([](pc::future<void>&& ready_f) {
    EXPECT_TRUE(ready_f.is_ready());
    ready_f.get();
    return "void value"s;
  });
  EXPECT_FALSE(f.valid());
  EXPECT_TRUE(string_f.valid());
  EXPECT_FALSE(string_f.is_ready());

  p.set_value();
  EXPECT_TRUE(string_f.is_ready());
  EXPECT_EQ(string_f.get(), "void value"s);
}

template<typename T>
void async_continuation_call() {
  auto f = set_value_in_other_thread<T>(25ms);
  ASSERT_TRUE(f.valid());

  pc::future<std::string> string_f = f.then([](pc::future<T>&& ready_f) {
    EXPECT_TRUE(ready_f.is_ready());
    return to_string(ready_f.get());
  });
  EXPECT_FALSE(f.valid());
  EXPECT_TRUE(string_f.valid());
  EXPECT_FALSE(string_f.is_ready());

  EXPECT_EQ(string_f.get(), to_string(some_value<T>()));
}

template<>
void async_continuation_call<void>() {
  auto f = set_value_in_other_thread<void>(25ms);
  ASSERT_TRUE(f.valid());

  pc::future<std::string> string_f = f.then([](pc::future<void>&& ready_f) {
    EXPECT_TRUE(ready_f.is_ready());
    ready_f.get();
    return "void value"s;
  });
  EXPECT_FALSE(f.valid());
  EXPECT_TRUE(string_f.valid());
  EXPECT_FALSE(string_f.is_ready());

  EXPECT_EQ(string_f.get(), "void value"s);
}

template<typename T>
void ready_continuation_call() {
  auto f = pc::make_ready_future(some_value<T>());

  pc::future<std::string> string_f = f.then([](pc::future<T>&& ready_f) {
    EXPECT_TRUE(ready_f.is_ready());
    return to_string(ready_f.get());
  });
  EXPECT_FALSE(f.valid());
  EXPECT_TRUE(string_f.valid());
  EXPECT_TRUE(string_f.is_ready());

  EXPECT_EQ(string_f.get(), to_string(some_value<T>()));
}

template<>
void ready_continuation_call<future_tests_env&>() {
  auto f = pc::make_ready_future(std::ref(some_value<future_tests_env&>()));

  pc::future<std::string> string_f = f.then([](pc::future<future_tests_env&>&& ready_f) {
    EXPECT_TRUE(ready_f.is_ready());
    return to_string(ready_f.get());
  });
  EXPECT_FALSE(f.valid());
  EXPECT_TRUE(string_f.valid());
  EXPECT_TRUE(string_f.is_ready());

  EXPECT_EQ(string_f.get(), to_string(some_value<future_tests_env&>()));
}

template<typename T>
void void_continuation() {
  auto f = set_value_in_other_thread<T>(25ms);
  bool executed = false;

  pc::future<void> void_f = f.then([&executed](pc::future<T>&& ready_f) -> void {
    EXPECT_TRUE(ready_f.is_ready());
    ready_f.get();
    executed = true;
  });
  EXPECT_FALSE(f.valid());
  EXPECT_TRUE(void_f.valid());
  EXPECT_FALSE(void_f.is_ready());

  EXPECT_NO_THROW(void_f.get());
  EXPECT_TRUE(executed);
}

template<typename T>
void ready_void_continuation() {
  auto f = pc::make_ready_future(some_value<T>());
  bool executed = false;

  pc::future<void> void_f = f.then([&executed](pc::future<T>&& ready_f) -> void {
    EXPECT_TRUE(ready_f.is_ready());
    ready_f.get();
    executed = true;
  });
  EXPECT_FALSE(f.valid());
  EXPECT_TRUE(void_f.valid());
  EXPECT_TRUE(void_f.is_ready());

  EXPECT_NO_THROW(void_f.get());
  EXPECT_TRUE(executed);
}

template<>
void ready_void_continuation<future_tests_env&>() {
  auto f = pc::make_ready_future(std::ref(some_value<future_tests_env&>()));
  bool executed = false;

  pc::future<void> void_f = f.then([&executed](pc::future<future_tests_env&>&& ready_f) -> void {
    EXPECT_TRUE(ready_f.is_ready());
    ready_f.get();
    executed = true;
  });
  EXPECT_FALSE(f.valid());
  EXPECT_TRUE(void_f.valid());
  EXPECT_TRUE(void_f.is_ready());

  EXPECT_NO_THROW(void_f.get());
  EXPECT_TRUE(executed);
}

template<>
void ready_void_continuation<void>() {
  auto f = pc::make_ready_future();
  bool executed = false;

  pc::future<void> void_f = f.then([&executed](pc::future<void>&& ready_f) -> void {
    EXPECT_TRUE(ready_f.is_ready());
    ready_f.get();
    executed = true;
  });
  EXPECT_FALSE(f.valid());
  EXPECT_TRUE(void_f.valid());
  EXPECT_TRUE(void_f.is_ready());

  EXPECT_NO_THROW(void_f.get());
  EXPECT_TRUE(executed);
}

template<>
void ready_continuation_call<void>() {
  auto f = pc::make_ready_future();

  pc::future<std::string> string_f = f.then([](pc::future<void>&& ready_f) {
    EXPECT_TRUE(ready_f.is_ready());
    ready_f.get();
    return "void value"s;
  });
  EXPECT_FALSE(f.valid());
  EXPECT_TRUE(string_f.valid());
  EXPECT_TRUE(string_f.is_ready());

  EXPECT_EQ(string_f.get(), "void value"s);
}

template<typename T>
void exception_to_continuation() {
  auto f = set_error_in_other_thread<T>(25ms, std::runtime_error("test error"));

  pc::future<std::string> string_f = f.then([](pc::future<T>&& ready_f) {
    EXPECT_TRUE(ready_f.is_ready());
    EXPECT_RUNTIME_ERROR(ready_f, "test error");
    return "Exception delivered"s;
  });
  EXPECT_FALSE(f.valid());
  EXPECT_TRUE(string_f.valid());
  EXPECT_FALSE(string_f.is_ready());

  EXPECT_EQ(string_f.get(), "Exception delivered"s);
}

template<typename T>
void exception_to_ready_continuation() {
  auto f = pc::make_exceptional_future<T>(std::runtime_error("test error"));

  pc::future<std::string> string_f = f.then([](pc::future<T>&& ready_f) {
    EXPECT_TRUE(ready_f.is_ready());
    EXPECT_RUNTIME_ERROR(ready_f, "test error");
    return "Exception delivered"s;
  });
  EXPECT_FALSE(f.valid());
  EXPECT_TRUE(string_f.valid());
  EXPECT_TRUE(string_f.is_ready());

  EXPECT_EQ(string_f.get(), "Exception delivered"s);
}

} // namespace tests

template<typename T>
struct FutureThen: ::testing::Test {
  pc::promise<T> promise;
  pc::future<T> future = promise.get_future();
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

TYPED_TEST(FutureThen, continuation_arg_is_ready) {
  bool arg_ready = false;
  auto cnt_future = this->future.then([&](pc::future<TypeParam> f) {arg_ready = f.is_ready();});
  set_promise_value(this->promise);
  EXPECT_TRUE(arg_ready);
}

TYPED_TEST(FutureThen, exception_from_continuation_delivered_to_returned_future) {
  pc::future<TypeParam> cnt_f = this->future.then([](pc::future<TypeParam>) -> TypeParam {
    throw std::runtime_error("continuation error");
  });
  set_promise_value(this->promise);
  EXPECT_RUNTIME_ERROR(cnt_f, "continuation error");
}

TYPED_TEST(FutureThen, exception_to_continuation) {tests::exception_to_continuation<TypeParam>();}
TYPED_TEST(FutureThen, exception_to_ready_continuation) {tests::exception_to_ready_continuation<TypeParam>();}
TYPED_TEST(FutureThen, continuation_call) {tests::continuation_call<TypeParam>();}
TYPED_TEST(FutureThen, async_continuation_call) {tests::async_continuation_call<TypeParam>();}
TYPED_TEST(FutureThen, ready_continuation_call) {tests::ready_continuation_call<TypeParam>();}
TYPED_TEST(FutureThen, void_continuation) {tests::void_continuation<TypeParam>();}
TYPED_TEST(FutureThen, ready_void_continuation) {tests::ready_void_continuation<TypeParam>();}

} // anonymous namespace
