#pragma once

#include <string>
#include <memory>

#include <gtest/gtest.h>

#include "concurrency/future"

#include "test_tools.h"
#include "test_helpers.h"

namespace {

using namespace std::literals;

template<typename T>
class ContinuationsTest: public ::testing::Test {};
TYPED_TEST_CASE_P(ContinuationsTest);

namespace tests {

template<typename T>
void exception_from_continuation() {
  concurrency::promise<T> p;
  auto f = p.get_future();
  ASSERT_TRUE(f.valid());

  concurrency::future<T> cont_f = f.then([](concurrency::future<T>&& ready_f) -> T {
    EXPECT_TRUE(ready_f.is_ready());
    throw std::runtime_error("continuation error");
  });
  EXPECT_FALSE(f.valid());
  EXPECT_TRUE(cont_f.valid());
  EXPECT_FALSE(cont_f.is_ready());

  p.set_value(some_value<T>());
  EXPECT_TRUE(cont_f.is_ready());
  EXPECT_RUNTIME_ERROR(cont_f, "continuation error");
}

template<>
void exception_from_continuation<void>() {
  concurrency::promise<void> p;
  auto f = p.get_future();
  ASSERT_TRUE(f.valid());

  concurrency::future<void> cont_f = f.then([](concurrency::future<void>&& ready_f) -> void {
    EXPECT_TRUE(ready_f.is_ready());
    throw std::runtime_error("continuation error");
  });
  EXPECT_FALSE(f.valid());
  EXPECT_TRUE(cont_f.valid());
  EXPECT_FALSE(cont_f.is_ready());

  p.set_value();
  EXPECT_TRUE(cont_f.is_ready());
  EXPECT_RUNTIME_ERROR(cont_f, "continuation error");
}

template<typename T>
void continuation_call() {
  concurrency::promise<T> p;
  auto f = p.get_future();
  ASSERT_TRUE(f.valid());

  concurrency::future<std::string> string_f = f.then([](concurrency::future<T>&& ready_f) {
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
  concurrency::promise<void> p;
  auto f = p.get_future();
  ASSERT_TRUE(f.valid());

  concurrency::future<std::string> string_f = f.then([](concurrency::future<void>&& ready_f) {
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

  concurrency::future<std::string> string_f = f.then([](concurrency::future<T>&& ready_f) {
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

  concurrency::future<std::string> string_f = f.then([](concurrency::future<void>&& ready_f) {
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
  auto f = concurrency::make_ready_future(some_value<T>());

  concurrency::future<std::string> string_f = f.then([](concurrency::future<T>&& ready_f) {
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
  auto f = concurrency::make_ready_future(std::ref(some_value<future_tests_env&>()));

  concurrency::future<std::string> string_f = f.then([](concurrency::future<future_tests_env&>&& ready_f) {
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

  concurrency::future<void> void_f = f.then([&executed](concurrency::future<T>&& ready_f) -> void {
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
  auto f = concurrency::make_ready_future(some_value<T>());
  bool executed = false;

  concurrency::future<void> void_f = f.then([&executed](concurrency::future<T>&& ready_f) -> void {
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
  auto f = concurrency::make_ready_future(std::ref(some_value<future_tests_env&>()));
  bool executed = false;

  concurrency::future<void> void_f = f.then([&executed](concurrency::future<future_tests_env&>&& ready_f) -> void {
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
  auto f = concurrency::make_ready_future();
  bool executed = false;

  concurrency::future<void> void_f = f.then([&executed](concurrency::future<void>&& ready_f) -> void {
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
  auto f = concurrency::make_ready_future();

  concurrency::future<std::string> string_f = f.then([](concurrency::future<void>&& ready_f) {
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

  concurrency::future<std::string> string_f = f.then([](concurrency::future<T>&& ready_f) {
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
  auto f = concurrency::make_exceptional_future<T>(std::runtime_error("test error"));

  concurrency::future<std::string> string_f = f.then([](concurrency::future<T>&& ready_f) {
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

TYPED_TEST_P(ContinuationsTest, exception_from_continuation) {tests::exception_from_continuation<TypeParam>();}
TYPED_TEST_P(ContinuationsTest, exception_to_continuation) {tests::exception_to_continuation<TypeParam>();}
TYPED_TEST_P(ContinuationsTest, exception_to_ready_continuation) {tests::exception_to_ready_continuation<TypeParam>();}
TYPED_TEST_P(ContinuationsTest, continuation_call) {tests::continuation_call<TypeParam>();}
TYPED_TEST_P(ContinuationsTest, async_continuation_call) {tests::async_continuation_call<TypeParam>();}
TYPED_TEST_P(ContinuationsTest, ready_continuation_call) {tests::ready_continuation_call<TypeParam>();}
TYPED_TEST_P(ContinuationsTest, void_continuation) {tests::void_continuation<TypeParam>();}
TYPED_TEST_P(ContinuationsTest, ready_void_continuation) {tests::ready_void_continuation<TypeParam>();}
REGISTER_TYPED_TEST_CASE_P(
  ContinuationsTest,
  exception_from_continuation,
  exception_to_continuation,
  exception_to_ready_continuation,
  continuation_call,
  async_continuation_call,
  ready_continuation_call,
  void_continuation,
  ready_void_continuation
);

INSTANTIATE_TYPED_TEST_CASE_P(VoidType, ContinuationsTest, void);
INSTANTIATE_TYPED_TEST_CASE_P(PrimitiveType, ContinuationsTest, int);
INSTANTIATE_TYPED_TEST_CASE_P(CopyableType, ContinuationsTest, std::string);
INSTANTIATE_TYPED_TEST_CASE_P(MoveableType, ContinuationsTest, std::unique_ptr<int>);
INSTANTIATE_TYPED_TEST_CASE_P(ReferenceType, ContinuationsTest, future_tests_env&);

} // anonymous namespace
