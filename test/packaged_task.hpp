#pragma once

#include <string>
#include <memory>

#include <gtest/gtest.h>

#include "concurrency/future"

#include "test_tools.h"

namespace {

template<typename T>
class PackagedTaskTest: public ::testing::Test {};
TYPED_TEST_CASE_P(PackagedTaskTest);

namespace tests {

template<typename T>
void get_task_future_twice() {
  experimental::packaged_task<T()> task(some_value<T>);
  experimental::future<T> f1, f2;
  EXPECT_NO_THROW(f1 = task.get_future());
  EXPECT_FUTURE_ERROR(
    f2 = task.get_future(),
    std::future_errc::future_already_retrieved
  );
}

template<typename T>
void successfull_call_makes_state_ready() {
  experimental::packaged_task<T()> task(some_value<T>);
  experimental::future<T> f = task.get_future();
  ASSERT_TRUE(f.valid());
  EXPECT_FALSE(f.is_ready());

  task();
  ASSERT_TRUE(f.valid());
  EXPECT_TRUE(f.is_ready());

  EXPECT_SOME_VALUE(f);
}

template<typename T>
void failed_call_makes_state_ready() {
  experimental::packaged_task<T()> task([]() -> T {
    throw std::runtime_error("operation failed");
  });
  experimental::future<T> f = task.get_future();
  ASSERT_TRUE(f.valid());
  EXPECT_FALSE(f.is_ready());

  EXPECT_NO_THROW(task());
  ASSERT_TRUE(f.valid());
  EXPECT_TRUE(f.is_ready());

  EXPECT_RUNTIME_ERROR(f, "operation failed");
}

} // namespace tests

TYPED_TEST_P(PackagedTaskTest, get_task_future_twice) {tests::get_task_future_twice<TypeParam>();}
TYPED_TEST_P(PackagedTaskTest, successfull_call_makes_state_ready) {tests::successfull_call_makes_state_ready<TypeParam>();}
TYPED_TEST_P(PackagedTaskTest, failed_call_makes_state_ready) {tests::failed_call_makes_state_ready<TypeParam>();}
REGISTER_TYPED_TEST_CASE_P(
  PackagedTaskTest,
  get_task_future_twice,
  successfull_call_makes_state_ready,
  failed_call_makes_state_ready
);

INSTANTIATE_TYPED_TEST_CASE_P(VoidType, PackagedTaskTest, void);
INSTANTIATE_TYPED_TEST_CASE_P(PrimitiveType, PackagedTaskTest, int);
INSTANTIATE_TYPED_TEST_CASE_P(CopyableType, PackagedTaskTest, std::string);
INSTANTIATE_TYPED_TEST_CASE_P(MoveableType, PackagedTaskTest, std::unique_ptr<int>);
INSTANTIATE_TYPED_TEST_CASE_P(ReferenceType, PackagedTaskTest, future_tests_env&);

} // anonymous namespace
