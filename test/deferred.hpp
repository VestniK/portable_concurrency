#pragma once

#include <memory>
#include <string>

#include <gtest/gtest.h>

#include "concurrency/future"

#include "test_tools.h"
#include "test_helpers.h"

namespace {

template<typename T>
class DeferredFutureTests: public ::testing::Test {};
TYPED_TEST_CASE_P(DeferredFutureTests);

namespace tests {

template<typename T>
void called_on_get() {
  unsigned call_count = 0;
  auto f = experimental::async(std::launch::deferred, [&call_count]() -> T {
    ++call_count;
    return some_value<T>();
  });
  ASSERT_TRUE(f.valid());
  EXPECT_EQ(0u, call_count);
  EXPECT_SOME_VALUE(f);
  EXPECT_EQ(1u, call_count);
}

template<typename T>
void called_on_shared_get() {
  unsigned call_count = 0;
  experimental::shared_future<T> sf1 = experimental::async(std::launch::deferred, [&call_count]() -> T {
    ++call_count;
    return some_value<T>();
  }).share();
  ASSERT_TRUE(sf1.valid());
  EXPECT_EQ(0u, call_count);

  experimental::shared_future<T> sf2 = sf1;
  EXPECT_EQ(0u, call_count);

  EXPECT_SOME_VALUE(sf1);
  EXPECT_EQ(1u, call_count);

  EXPECT_SOME_VALUE(sf2);
  EXPECT_EQ(1u, call_count);
}

template<typename T>
void copy_shared_after_get() {
  unsigned call_count = 0;
  experimental::shared_future<T> sf1 = experimental::async(std::launch::deferred, [&call_count]() -> T {
    ++call_count;
    return some_value<T>();
  }).share();
  ASSERT_TRUE(sf1.valid());
  EXPECT_EQ(0u, call_count);

  EXPECT_SOME_VALUE(sf1);
  EXPECT_EQ(1u, call_count);

  experimental::shared_future<T> sf2 = sf1;
  EXPECT_EQ(1u, call_count);

  EXPECT_SOME_VALUE(sf2);
  EXPECT_EQ(1u, call_count);
}

} // namespace test

TYPED_TEST_P(DeferredFutureTests, called_on_get) {tests::called_on_get<TypeParam>();}
TYPED_TEST_P(DeferredFutureTests, called_on_shared_get) {tests::called_on_shared_get<TypeParam>();}
TYPED_TEST_P(DeferredFutureTests, copy_shared_after_get) {tests::copy_shared_after_get<TypeParam>();}
REGISTER_TYPED_TEST_CASE_P(
  DeferredFutureTests,
  called_on_get,
  called_on_shared_get,
  copy_shared_after_get
);

// TODO: INSTANTIATE_TYPED_TEST_CASE_P(VoidType, DeferredFutureTests, void);
INSTANTIATE_TYPED_TEST_CASE_P(PrimitiveType, DeferredFutureTests, int);
INSTANTIATE_TYPED_TEST_CASE_P(CopyableType, DeferredFutureTests, std::string);
INSTANTIATE_TYPED_TEST_CASE_P(MoveableType, DeferredFutureTests, std::unique_ptr<int>);
INSTANTIATE_TYPED_TEST_CASE_P(ReferenceType, DeferredFutureTests, future_tests_env&);

} // anonymous namespace
