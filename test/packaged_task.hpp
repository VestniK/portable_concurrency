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
void default_constructed_is_invalid() {
  experimental::packaged_task<T()> task;
  EXPECT_FALSE(task.valid());
}

template<typename T>
void moved_to_constructor_is_invalid() {
  experimental::packaged_task<T()> task1(some_value<T>);
  EXPECT_TRUE(task1.valid());

  experimental::packaged_task<T()> task2(std::move(task1));
  EXPECT_FALSE(task1.valid());
  EXPECT_TRUE(task2.valid());
}

template<typename T>
void moved_to_assigment_is_invalid() {
  experimental::packaged_task<T()> task1(some_value<T>);
  experimental::packaged_task<T()> task2;
  EXPECT_TRUE(task1.valid());
  EXPECT_FALSE(task2.valid());

  task2 = std::move(task1);
  EXPECT_FALSE(task1.valid());
  EXPECT_TRUE(task2.valid());
}

template<typename T>
void swap_valid_task_with_invalid() {
  experimental::packaged_task<T()> task1(some_value<T>);
  experimental::packaged_task<T()> task2;
  EXPECT_TRUE(task1.valid());
  EXPECT_FALSE(task2.valid());

  task2.swap(task1);
  EXPECT_FALSE(task1.valid());
  EXPECT_TRUE(task2.valid());
}

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

template<typename T>
void swap_valid_tasks() {
  experimental::packaged_task<T()> task1(some_value<T>);
  experimental::packaged_task<T()> task2(some_value<T>);
  ASSERT_TRUE(task1.valid());
  ASSERT_TRUE(task2.valid());

  auto f1 = task1.get_future(), f2 = task2.get_future();
  ASSERT_TRUE(f1.valid());
  EXPECT_FALSE(f1.is_ready());
  ASSERT_TRUE(f2.valid());
  EXPECT_FALSE(f2.is_ready());

  task2.swap(task1);

  task1();
  EXPECT_FALSE(f1.is_ready());
  EXPECT_TRUE(f2.is_ready());

  task2();
  EXPECT_TRUE(f1.is_ready());
  EXPECT_TRUE(f2.is_ready());
}

} // namespace tests

TYPED_TEST_P(PackagedTaskTest, default_constructed_is_invalid) {tests::default_constructed_is_invalid<TypeParam>();}
TYPED_TEST_P(PackagedTaskTest, moved_to_constructor_is_invalid) {tests::moved_to_constructor_is_invalid<TypeParam>();}
TYPED_TEST_P(PackagedTaskTest, moved_to_assigment_is_invalid) {tests::moved_to_assigment_is_invalid<TypeParam>();}
TYPED_TEST_P(PackagedTaskTest, swap_valid_task_with_invalid) {tests::swap_valid_task_with_invalid<TypeParam>();}
TYPED_TEST_P(PackagedTaskTest, get_task_future_twice) {tests::get_task_future_twice<TypeParam>();}
TYPED_TEST_P(PackagedTaskTest, successfull_call_makes_state_ready) {tests::successfull_call_makes_state_ready<TypeParam>();}
TYPED_TEST_P(PackagedTaskTest, failed_call_makes_state_ready) {tests::failed_call_makes_state_ready<TypeParam>();}
TYPED_TEST_P(PackagedTaskTest, swap_valid_tasks) {tests::swap_valid_tasks<TypeParam>();}
REGISTER_TYPED_TEST_CASE_P(
  PackagedTaskTest,
  default_constructed_is_invalid,
  moved_to_constructor_is_invalid,
  moved_to_assigment_is_invalid,
  swap_valid_task_with_invalid,
  get_task_future_twice,
  successfull_call_makes_state_ready,
  failed_call_makes_state_ready,
  swap_valid_tasks
);

INSTANTIATE_TYPED_TEST_CASE_P(VoidType, PackagedTaskTest, void);
INSTANTIATE_TYPED_TEST_CASE_P(PrimitiveType, PackagedTaskTest, int);
INSTANTIATE_TYPED_TEST_CASE_P(CopyableType, PackagedTaskTest, std::string);
INSTANTIATE_TYPED_TEST_CASE_P(MoveableType, PackagedTaskTest, std::unique_ptr<int>);
INSTANTIATE_TYPED_TEST_CASE_P(ReferenceType, PackagedTaskTest, future_tests_env&);

} // anonymous namespace
