#pragma once

#include <string>
#include <memory>

#include <gtest/gtest.h>

#include "concurrency/future"

#include "test_tools.h"

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
void stringify_continuation() {static_assert(sizeof(T) == 0, "stringify_continuation<T> is deleted");} // = delete; in C++ but not in clang++

template<>
void stringify_continuation<void>() {
  concurrency::promise<void> p;
  auto f = p.get_future();
  ASSERT_TRUE(f.valid());

  concurrency::future<std::string> string_f = f.then([](concurrency::future<void>&& ready_f) {
    EXPECT_TRUE(ready_f.is_ready());
    return "void value"s;
  });
  EXPECT_FALSE(f.valid());
  EXPECT_TRUE(string_f.valid());
  EXPECT_FALSE(string_f.is_ready());

  p.set_value();
  EXPECT_TRUE(string_f.is_ready());
  EXPECT_EQ(string_f.get(), "void value"s);
}

template<>
void stringify_continuation<int>() {
  concurrency::promise<int> p;
  auto f = p.get_future();
  ASSERT_TRUE(f.valid());

  concurrency::future<std::string> string_f = f.then([](concurrency::future<int>&& ready_f) {
    EXPECT_TRUE(ready_f.is_ready());
    return std::to_string(ready_f.get());
  });
  EXPECT_FALSE(f.valid());
  EXPECT_TRUE(string_f.valid());
  EXPECT_FALSE(string_f.is_ready());

  p.set_value(42);
  EXPECT_TRUE(string_f.is_ready());
  EXPECT_EQ(string_f.get(), "42"s);
}

template<>
void stringify_continuation<std::string>() {
  concurrency::promise<std::string> p;
  auto f = p.get_future();
  ASSERT_TRUE(f.valid());

  concurrency::future<std::string> string_f = f.then([](concurrency::future<std::string>&& ready_f) {
    EXPECT_TRUE(ready_f.is_ready());
    return "'" + ready_f.get() + "'";
  });
  EXPECT_FALSE(f.valid());
  EXPECT_TRUE(string_f.valid());
  EXPECT_FALSE(string_f.is_ready());

  p.set_value("hello");
  EXPECT_TRUE(string_f.is_ready());
  EXPECT_EQ(string_f.get(), "'hello'"s);
}

template<>
void stringify_continuation<std::unique_ptr<int>>() {
  concurrency::promise<std::unique_ptr<int>> p;
  auto f = p.get_future();
  ASSERT_TRUE(f.valid());

  concurrency::future<std::string> string_f = f.then([](concurrency::future<std::unique_ptr<int>>&& ready_f) {
    EXPECT_TRUE(ready_f.is_ready());
    return std::to_string(*ready_f.get());
  });
  EXPECT_FALSE(f.valid());
  EXPECT_TRUE(string_f.valid());
  EXPECT_FALSE(string_f.is_ready());

  p.set_value(std::make_unique<int>(42));
  EXPECT_TRUE(string_f.is_ready());
  EXPECT_EQ(string_f.get(), "42"s);
}

template<>
void stringify_continuation<future_tests_env&>() {
  concurrency::promise<future_tests_env&> p;
  auto f = p.get_future();
  ASSERT_TRUE(f.valid());

  concurrency::future<std::string> string_f = f.then([](concurrency::future<future_tests_env&>&& ready_f) {
    EXPECT_TRUE(ready_f.is_ready());
    return std::to_string(reinterpret_cast<uintptr_t>(&ready_f.get()));
  });
  EXPECT_FALSE(f.valid());
  EXPECT_TRUE(string_f.valid());
  EXPECT_FALSE(string_f.is_ready());

  p.set_value(*g_future_tests_env);
  EXPECT_TRUE(string_f.is_ready());
  EXPECT_EQ(string_f.get(), std::to_string(reinterpret_cast<uintptr_t>(g_future_tests_env)));
}

} // namespace tests

TYPED_TEST_P(ContinuationsTest, stringify_continuation) {tests::stringify_continuation<TypeParam>();}
TYPED_TEST_P(ContinuationsTest, exception_from_continuation) {tests::exception_from_continuation<TypeParam>();}
REGISTER_TYPED_TEST_CASE_P(
  ContinuationsTest,
  stringify_continuation,
  exception_from_continuation
);

INSTANTIATE_TYPED_TEST_CASE_P(VoidType, ContinuationsTest, void);
INSTANTIATE_TYPED_TEST_CASE_P(PrimitiveType, ContinuationsTest, int);
INSTANTIATE_TYPED_TEST_CASE_P(CopyableType, ContinuationsTest, std::string);
INSTANTIATE_TYPED_TEST_CASE_P(MoveableType, ContinuationsTest, std::unique_ptr<int>);
INSTANTIATE_TYPED_TEST_CASE_P(ReferenceType, ContinuationsTest, future_tests_env&);

} // anonymous namespace
