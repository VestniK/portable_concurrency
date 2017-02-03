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

template<typename T>
void one_param_deferred_func() {
  unsigned call_count = 0;
  auto f = experimental::async(
    std::launch::deferred,
    [](unsigned& counter) -> T {
      ++counter;
      return some_value<T>();
    },
    std::ref(call_count)
  );
  ASSERT_TRUE(f.valid());
  EXPECT_EQ(0u, call_count);
  EXPECT_SOME_VALUE(f);
  EXPECT_EQ(1u, call_count);
}

template<typename T>
void move_only_param_deferred_func() {
  unsigned call_count = 0;
  experimental::promise<std::string> p;
  auto pf = p.get_future();
  auto df = experimental::async(
    std::launch::deferred,
    [&call_count](experimental::promise<std::string>&& promise) -> T {
      ++call_count;
      promise.set_value("deferred action launched");
      return some_value<T>();
    },
    std::move(p)
  );
  ASSERT_TRUE(df.valid());
  EXPECT_EQ(0u, call_count);
  EXPECT_FALSE(df.is_ready());
  EXPECT_SOME_VALUE(df);
  EXPECT_EQ(1u, call_count);

  EXPECT_TRUE(pf.is_ready());
  EXPECT_EQ("deferred action launched", pf.get());
}

template<typename T>
void two_params_deferred_func() {
  unsigned call_count = 0;
  experimental::promise<std::string> p;
  auto pf = p.get_future();
  auto df = experimental::async(
    std::launch::deferred,
    [](experimental::promise<std::string>&& promise, unsigned& counter) -> T {
      ++counter;
      promise.set_value("deferred action launched");
      return some_value<T>();
    },
    std::move(p),
    std::ref(call_count)
  );
  ASSERT_TRUE(df.valid());
  EXPECT_EQ(0u, call_count);
  EXPECT_FALSE(df.is_ready());
  EXPECT_SOME_VALUE(df);
  EXPECT_EQ(1u, call_count);

  EXPECT_TRUE(pf.is_ready());
  EXPECT_EQ("deferred action launched", pf.get());
}

template<typename T>
void deferred_future_continuation() {
  unsigned call_count = 0;
  unsigned continuation_call_count = 0;
  auto f = experimental::async(std::launch::deferred, [&call_count]() -> T {
    ++call_count;
    return some_value<T>();
  }).then([&continuation_call_count](experimental::future<T>&& ready) {
    ++continuation_call_count;
    EXPECT_TRUE(ready.is_ready());
    return to_string(ready.get());
  });
  ASSERT_TRUE(f.valid());

  EXPECT_EQ(0u, call_count);
  EXPECT_EQ(0u, continuation_call_count);

  EXPECT_EQ(to_string(some_value<T>()), f.get());
  EXPECT_EQ(1u, call_count);
  EXPECT_EQ(1u, continuation_call_count);
}

template<>
void deferred_future_continuation<void>() {
  unsigned call_count = 0;
  unsigned continuation_call_count = 0;
  auto f = experimental::async(std::launch::deferred, [&call_count]() -> void {
    ++call_count;
  }).then([&continuation_call_count](experimental::future<void>&& ready) {
    ++continuation_call_count;
    EXPECT_TRUE(ready.is_ready());
    EXPECT_NO_THROW(ready.get());
    return "void val"s;
  });
  ASSERT_TRUE(f.valid());

  EXPECT_EQ(0u, call_count);
  EXPECT_EQ(0u, continuation_call_count);

  EXPECT_EQ("void val"s, f.get());
  EXPECT_EQ(1u, call_count);
  EXPECT_EQ(1u, continuation_call_count);
}

} // namespace test

TYPED_TEST_P(DeferredFutureTests, called_on_get) {tests::called_on_get<TypeParam>();}
TYPED_TEST_P(DeferredFutureTests, called_on_shared_get) {tests::called_on_shared_get<TypeParam>();}
TYPED_TEST_P(DeferredFutureTests, copy_shared_after_get) {tests::copy_shared_after_get<TypeParam>();}
TYPED_TEST_P(DeferredFutureTests, one_param_deferred_func) {tests::one_param_deferred_func<TypeParam>();}
TYPED_TEST_P(DeferredFutureTests, move_only_param_deferred_func) {tests::move_only_param_deferred_func<TypeParam>();}
TYPED_TEST_P(DeferredFutureTests, two_params_deferred_func) {tests::two_params_deferred_func<TypeParam>();}
TYPED_TEST_P(DeferredFutureTests, deferred_future_continuation) {tests::deferred_future_continuation<TypeParam>();}
REGISTER_TYPED_TEST_CASE_P(
  DeferredFutureTests,
  called_on_get,
  called_on_shared_get,
  copy_shared_after_get,
  one_param_deferred_func,
  move_only_param_deferred_func,
  two_params_deferred_func,
  deferred_future_continuation
);

INSTANTIATE_TYPED_TEST_CASE_P(VoidType, DeferredFutureTests, void);
INSTANTIATE_TYPED_TEST_CASE_P(PrimitiveType, DeferredFutureTests, int);
INSTANTIATE_TYPED_TEST_CASE_P(CopyableType, DeferredFutureTests, std::string);
INSTANTIATE_TYPED_TEST_CASE_P(MoveableType, DeferredFutureTests, std::unique_ptr<int>);
INSTANTIATE_TYPED_TEST_CASE_P(ReferenceType, DeferredFutureTests, future_tests_env&);

} // anonymous namespace
