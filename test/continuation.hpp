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
T some_value() = delete;

template<>
int some_value<int>() {return 42;}

template<>
std::string some_value<std::string>() {return "hello";}

template<>
std::unique_ptr<int> some_value<std::unique_ptr<int>>() {return std::make_unique<int>(42);}

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
void stringify_continuation() = delete;

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

} // anonymous namespace
