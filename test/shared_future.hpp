#pragma once

#include <chrono>
#include <iostream>
#include <memory>
#include <string>
#include <thread>

#include <gtest/gtest.h>

#include "concurrency/future"

#include "test_tools.h"

using namespace std::literals;
using sys_clock = std::chrono::system_clock;
using hires_clock = std::chrono::high_resolution_clock;

namespace {

template<typename T>
class SharedFutureTests: public ::testing::Test {};
TYPED_TEST_CASE_P(SharedFutureTests);

TYPED_TEST_P(SharedFutureTests, default_constructed_is_invalid) {
  concurrency::shared_future<TypeParam> future;
  EXPECT_FALSE(future.valid());
}

TYPED_TEST_P(SharedFutureTests, obtained_from_promise_is_valid) {
  concurrency::promise<TypeParam> promise;
  auto future = promise.get_future().share();
  EXPECT_TRUE(future.valid());
}

TYPED_TEST_P(SharedFutureTests, copy_constructed_from_invalid_is_invalid) {
  concurrency::shared_future<TypeParam> sf1;
  ASSERT_FALSE(sf1.valid());
  concurrency::shared_future<TypeParam> sf2 = sf1;
  EXPECT_FALSE(sf1.valid());
  EXPECT_FALSE(sf2.valid());
}

TYPED_TEST_P(SharedFutureTests, copy_assigned_from_invalid_is_invalid) {
  concurrency::shared_future<TypeParam> sf1;
  concurrency::shared_future<TypeParam> sf2;
  ASSERT_FALSE(sf1.valid());
  ASSERT_FALSE(sf2.valid());

  sf2 = sf1;
  EXPECT_FALSE(sf1.valid());
  EXPECT_FALSE(sf2.valid());
}

TYPED_TEST_P(SharedFutureTests, copy_constructed_from_valid_is_valid) {
  concurrency::promise<TypeParam> p;
  concurrency::shared_future<TypeParam> sf1 = p.get_future();
  ASSERT_TRUE(sf1.valid());
  concurrency::shared_future<TypeParam> sf2 = sf1;
  EXPECT_TRUE(sf1.valid());
  EXPECT_TRUE(sf2.valid());
}

TYPED_TEST_P(SharedFutureTests, copy_assigned_from_valid_is_valid) {
  concurrency::promise<TypeParam> p;
  concurrency::shared_future<TypeParam> sf1 = p.get_future();
  concurrency::shared_future<TypeParam> sf2;
  ASSERT_TRUE(sf1.valid());
  ASSERT_FALSE(sf2.valid());

  sf2 = sf1;
  EXPECT_TRUE(sf1.valid());
  EXPECT_TRUE(sf2.valid());
}

TYPED_TEST_P(SharedFutureTests, moved_to_constructor_is_invalid) {
  concurrency::promise<TypeParam> promise;
  concurrency::shared_future<TypeParam> future = promise.get_future().share();
  EXPECT_TRUE(future.valid());
  concurrency::shared_future<TypeParam> another_future{std::move(future)};
  EXPECT_FALSE(future.valid());
  EXPECT_TRUE(another_future.valid());
}

TYPED_TEST_P(SharedFutureTests, moved_to_assigment_to_invalid_is_invalid) {
  concurrency::promise<TypeParam> promise;
  concurrency::shared_future<TypeParam> future = promise.get_future().share();
  EXPECT_TRUE(future.valid());
  concurrency::shared_future<TypeParam> another_future;
  EXPECT_FALSE(another_future.valid());
  another_future= std::move(future);
  EXPECT_FALSE(future.valid());
  EXPECT_TRUE(another_future.valid());
}

TYPED_TEST_P(SharedFutureTests, moved_to_assigment_to_valid_is_invalid) {
  concurrency::promise<TypeParam> promise1;
  concurrency::promise<TypeParam> promise2;
  concurrency::shared_future<TypeParam> future1 = promise1.get_future().share();
  concurrency::shared_future<TypeParam> future2 = promise2.get_future().share();
  EXPECT_TRUE(future1.valid());
  EXPECT_TRUE(future2.valid());

  future2 = std::move(future1);
  EXPECT_TRUE(future2.valid());
  EXPECT_FALSE(future1.valid());
}

TYPED_TEST_P(SharedFutureTests, move_constructed_from_invalid_future) {
  concurrency::future<TypeParam> f;
  ASSERT_FALSE(f.valid());
  concurrency::shared_future<TypeParam> sf(std::move(f));
  EXPECT_FALSE(sf.valid());
  EXPECT_FALSE(f.valid());
}

TYPED_TEST_P(SharedFutureTests, move_constructed_from_valid_future) {
  concurrency::promise<TypeParam> p;
  auto f = p.get_future();
  ASSERT_TRUE(f.valid());
  concurrency::shared_future<TypeParam> sf(std::move(f));
  EXPECT_TRUE(sf.valid());
  EXPECT_FALSE(f.valid());
}

TYPED_TEST_P(SharedFutureTests, share_of_invalid_is_invalid) {
  concurrency::future<TypeParam> future;
  ASSERT_FALSE(future.valid());
  concurrency::shared_future<TypeParam> shared = future.share();
  EXPECT_FALSE(shared.valid());
}

TYPED_TEST_P(SharedFutureTests, is_ready_on_nonready) {
  concurrency::promise<TypeParam> promise;
  concurrency::shared_future<TypeParam> future = promise.get_future().share();
  EXPECT_FALSE(future.is_ready());
}

TYPED_TEST_P(SharedFutureTests, is_ready_on_future_with_value) {
  concurrency::promise<TypeParam> promise;
  concurrency::shared_future<TypeParam> future = promise.get_future().share();
  set_promise_value<TypeParam>(promise);
  EXPECT_TRUE(future.is_ready());
}

TYPED_TEST_P(SharedFutureTests, is_ready_on_future_with_error) {
  concurrency::promise<TypeParam> promise;
  concurrency::shared_future<TypeParam> future = promise.get_future().share();
  promise.set_exception(std::make_exception_ptr(std::runtime_error("test error")));
  EXPECT_TRUE(future.is_ready());
}

TYPED_TEST_P(SharedFutureTests, get_on_invalid) {
  concurrency::shared_future<TypeParam> future;
  ASSERT_FALSE(future.valid());
  EXPECT_FUTURE_ERROR(future.get(), std::future_errc::no_state);
}

template<typename T>
void test_retrieve_shared_future_result() {
  concurrency::shared_future<T> future = set_value_in_other_thread<T>(50ms).share();
  ASSERT_TRUE(future.valid());

  EXPECT_EQ(some_value<T>(), future.get());
  EXPECT_TRUE(future.valid()); // TODO: investigate if shared_future must remain valid after get()
}

template<>
void test_retrieve_shared_future_result<std::unique_ptr<int>>() {
  auto future = set_value_in_other_thread<std::unique_ptr<int>>(50ms).share();
  ASSERT_TRUE(future.valid());

  EXPECT_EQ(42, *future.get());
  EXPECT_TRUE(future.valid()); // TODO: investigate if shared_future must remain valid after get()
}

template<>
void test_retrieve_shared_future_result<void>() {
  auto future = set_value_in_other_thread<void>(50ms).share();
  ASSERT_TRUE(future.valid());

  EXPECT_NO_THROW(future.get());
  EXPECT_TRUE(future.valid()); // TODO: investigate if shared_future must remain valid after get()
}

template<>
void test_retrieve_shared_future_result<future_tests_env&>() {
  auto future = set_value_in_other_thread<future_tests_env&>(50ms).share();
  ASSERT_TRUE(future.valid());

  EXPECT_EQ(g_future_tests_env, &future.get());
  EXPECT_TRUE(future.valid()); // TODO: investigate if shared_future must remain valid after get()
}

TYPED_TEST_P(SharedFutureTests, retrieve_result) {
  test_retrieve_shared_future_result<TypeParam>();
}

TYPED_TEST_P(SharedFutureTests, retrieve_exception) {
  auto future = set_error_in_other_thread<TypeParam>(50ms, std::runtime_error("test error")).share();
  ASSERT_TRUE(future.valid());

  EXPECT_RUNTIME_ERROR(future, "test error");
  EXPECT_TRUE(future.valid());
}

template<typename T>
void test_retrieve_shared_future_result_twice() {
  concurrency::shared_future<T> sf1 = set_value_in_other_thread<T>(50ms);
  auto sf2 = sf1;
  ASSERT_TRUE(sf1.valid());
  ASSERT_TRUE(sf2.valid());

  EXPECT_EQ(some_value<T>(), sf1.get());
  EXPECT_EQ(some_value<T>(), sf2.get());
  EXPECT_TRUE(sf1.valid()); // TODO: investigate if shared_future must remain valid after get()
  EXPECT_TRUE(sf2.valid()); // TODO: investigate if shared_future must remain valid after get()
}

template<>
void test_retrieve_shared_future_result_twice<std::unique_ptr<int>>() {
  concurrency::shared_future<std::unique_ptr<int>> sf1 = set_value_in_other_thread<std::unique_ptr<int>>(50ms);
  auto sf2 = sf1;
  ASSERT_TRUE(sf1.valid());
  ASSERT_TRUE(sf2.valid());

  EXPECT_EQ(42, *sf1.get());
  EXPECT_EQ(42, *sf2.get());
  EXPECT_TRUE(sf1.valid()); // TODO: investigate if shared_future must remain valid after get()
  EXPECT_TRUE(sf2.valid()); // TODO: investigate if shared_future must remain valid after get()
}

template<>
void test_retrieve_shared_future_result_twice<void>() {
  concurrency::shared_future<void> sf1 = set_value_in_other_thread<void>(50ms);
  auto sf2 = sf1;
  ASSERT_TRUE(sf1.valid());
  ASSERT_TRUE(sf2.valid());

  EXPECT_NO_THROW(sf1.get());
  EXPECT_NO_THROW(sf2.get());
  EXPECT_TRUE(sf1.valid()); // TODO: investigate if shared_future must remain valid after get()
  EXPECT_TRUE(sf2.valid()); // TODO: investigate if shared_future must remain valid after get()
}

template<>
void test_retrieve_shared_future_result_twice<future_tests_env&>() {
  concurrency::shared_future<future_tests_env&> sf1 = set_value_in_other_thread<future_tests_env&>(50ms);
  auto sf2 = sf1;
  ASSERT_TRUE(sf1.valid());
  ASSERT_TRUE(sf2.valid());

  EXPECT_EQ(g_future_tests_env, &sf1.get());
  EXPECT_EQ(g_future_tests_env, &sf2.get());
  EXPECT_TRUE(sf1.valid()); // TODO: investigate if shared_future must remain valid after get()
  EXPECT_TRUE(sf2.valid()); // TODO: investigate if shared_future must remain valid after get()
}

TYPED_TEST_P(SharedFutureTests, retreive_result_from_several_futures) {
  test_retrieve_shared_future_result_twice<TypeParam>();
}

TYPED_TEST_P(SharedFutureTests, retreive_exception_from_several_futures) {
  concurrency::shared_future<TypeParam> sf1 =
    set_error_in_other_thread<TypeParam>(50ms, std::runtime_error("test error"))
  ;
  auto sf2 = sf1;
  ASSERT_TRUE(sf1.valid());
  ASSERT_TRUE(sf2.valid());

  EXPECT_RUNTIME_ERROR(sf1, "test error");
  EXPECT_RUNTIME_ERROR(sf2, "test error");
  EXPECT_TRUE(sf1.valid());
  EXPECT_TRUE(sf2.valid());
}

TYPED_TEST_P(SharedFutureTests, wait_on_invalid) {
  concurrency::shared_future<TypeParam> future;
  ASSERT_FALSE(future.valid());

  EXPECT_FUTURE_ERROR(future.wait(), std::future_errc::no_state);
}

TYPED_TEST_P(SharedFutureTests, wait_for_on_invalid) {
  concurrency::shared_future<TypeParam> future;
  ASSERT_FALSE(future.valid());

  EXPECT_FUTURE_ERROR(future.wait_for(5s), std::future_errc::no_state);
}

TYPED_TEST_P(SharedFutureTests, wait_until_on_invalid) {
  concurrency::shared_future<TypeParam> future;
  ASSERT_FALSE(future.valid());

  EXPECT_FUTURE_ERROR(
    future.wait_until(sys_clock::now() + 5s),
    std::future_errc::no_state
  );
}

TYPED_TEST_P(SharedFutureTests, wait_on_ready_value) {
  concurrency::promise<TypeParam> promise;
  auto future = promise.get_future().share();
  set_promise_value<TypeParam>(promise);
  ASSERT_TRUE(future.valid());
  ASSERT_TRUE(future.is_ready());

  EXPECT_NO_THROW(future.wait());
  EXPECT_TRUE(future.valid());
  EXPECT_TRUE(future.is_ready());

  EXPECT_EQ(std::future_status::ready, future.wait_for(5s));
  EXPECT_TRUE(future.valid());
  EXPECT_TRUE(future.is_ready());

  EXPECT_EQ(std::future_status::ready, future.wait_until(
    sys_clock::now() + 5s
  ));
  EXPECT_TRUE(future.valid());
  EXPECT_TRUE(future.is_ready());
}

TYPED_TEST_P(SharedFutureTests, wait_on_ready_error) {
  concurrency::promise<TypeParam> promise;
  auto future = promise.get_future().share();
  promise.set_exception(std::make_exception_ptr(std::runtime_error("test error")));
  ASSERT_TRUE(future.valid());
  ASSERT_TRUE(future.is_ready());

  EXPECT_NO_THROW(future.wait());
  EXPECT_TRUE(future.valid());
  EXPECT_TRUE(future.is_ready());

  EXPECT_EQ(std::future_status::ready, future.wait_for(5s));
  EXPECT_TRUE(future.valid());
  EXPECT_TRUE(future.is_ready());

  EXPECT_EQ(std::future_status::ready, future.wait_until(
    sys_clock::now() + 5s
  ));
  EXPECT_TRUE(future.valid());
  EXPECT_TRUE(future.is_ready());
}

TYPED_TEST_P(SharedFutureTests, wait_timeout) {
  concurrency::promise<TypeParam> promise;
  auto future = promise.get_future().share();
  ASSERT_TRUE(future.valid());
  ASSERT_FALSE(future.is_ready());

  EXPECT_EQ(std::future_status::timeout, future.wait_for(10ms));
  EXPECT_TRUE(future.valid());
  EXPECT_FALSE(future.is_ready());

  EXPECT_EQ(std::future_status::timeout, future.wait_until(
    hires_clock::now() + 10ms
  ));
  EXPECT_TRUE(future.valid());
  EXPECT_FALSE(future.is_ready());
}

TYPED_TEST_P(SharedFutureTests, wait_awakes_on_value) {
  auto future = set_value_in_other_thread<TypeParam>(50ms).share();
  ASSERT_TRUE(future.valid());
  ASSERT_FALSE(future.is_ready());

  EXPECT_NO_THROW(future.wait());
  EXPECT_TRUE(future.valid());
  EXPECT_TRUE(future.is_ready());
}

TYPED_TEST_P(SharedFutureTests, wait_for_awakes_on_value) {
  auto future = set_value_in_other_thread<TypeParam>(50ms).share();
  ASSERT_TRUE(future.valid());
  ASSERT_FALSE(future.is_ready());

  EXPECT_NO_THROW(future.wait_for(15s));
  EXPECT_TRUE(future.valid());
  EXPECT_TRUE(future.is_ready());
}

TYPED_TEST_P(SharedFutureTests, wait_until_awakes_on_value) {
  auto future = set_value_in_other_thread<TypeParam>(50ms).share();
  ASSERT_TRUE(future.valid());
  ASSERT_FALSE(future.is_ready());

  EXPECT_NO_THROW(future.wait_until(sys_clock::now() + 15s));
  EXPECT_TRUE(future.valid());
  EXPECT_TRUE(future.is_ready());
}

TYPED_TEST_P(SharedFutureTests, wait_awakes_on_error) {
  auto future = set_error_in_other_thread<TypeParam>(50ms, std::runtime_error("test error")).share();
  ASSERT_TRUE(future.valid());
  ASSERT_FALSE(future.is_ready());

  EXPECT_NO_THROW(future.wait());
  EXPECT_TRUE(future.valid());
  EXPECT_TRUE(future.is_ready());
}

TYPED_TEST_P(SharedFutureTests, wait_for_awakes_on_error) {
  auto future = set_error_in_other_thread<TypeParam>(50ms, std::runtime_error("test error")).share();
  ASSERT_TRUE(future.valid());
  ASSERT_FALSE(future.is_ready());

  EXPECT_NO_THROW(future.wait_for(15s));
  EXPECT_TRUE(future.valid());
  EXPECT_TRUE(future.is_ready());
}

TYPED_TEST_P(SharedFutureTests, wait_until_awakes_on_error) {
  auto future = set_error_in_other_thread<TypeParam>(50ms, std::runtime_error("test error")).share();
  ASSERT_TRUE(future.valid());
  ASSERT_FALSE(future.is_ready());

  EXPECT_NO_THROW(future.wait_until(sys_clock::now() + 15s));
  EXPECT_TRUE(future.valid());
  EXPECT_TRUE(future.is_ready());
}

REGISTER_TYPED_TEST_CASE_P(
  SharedFutureTests,
  default_constructed_is_invalid,
  obtained_from_promise_is_valid,
  copy_constructed_from_invalid_is_invalid,
  copy_assigned_from_invalid_is_invalid,
  copy_constructed_from_valid_is_valid,
  copy_assigned_from_valid_is_valid,
  moved_to_constructor_is_invalid,
  moved_to_assigment_to_invalid_is_invalid,
  moved_to_assigment_to_valid_is_invalid,
  move_constructed_from_invalid_future,
  move_constructed_from_valid_future,
  share_of_invalid_is_invalid,
  is_ready_on_nonready,
  is_ready_on_future_with_value,
  is_ready_on_future_with_error,
  get_on_invalid,
  retrieve_result,
  retrieve_exception,
  retreive_result_from_several_futures,
  retreive_exception_from_several_futures,
  wait_on_invalid,
  wait_for_on_invalid,
  wait_until_on_invalid,
  wait_on_ready_value,
  wait_on_ready_error,
  wait_timeout,
  wait_awakes_on_value,
  wait_for_awakes_on_value,
  wait_until_awakes_on_value,
  wait_awakes_on_error,
  wait_for_awakes_on_error,
  wait_until_awakes_on_error
);

INSTANTIATE_TYPED_TEST_CASE_P(VoidType, SharedFutureTests, void);
INSTANTIATE_TYPED_TEST_CASE_P(PrimitiveType, SharedFutureTests, int);
INSTANTIATE_TYPED_TEST_CASE_P(CopyableType, SharedFutureTests, std::string);
INSTANTIATE_TYPED_TEST_CASE_P(MoveableType, SharedFutureTests, std::unique_ptr<int>);
INSTANTIATE_TYPED_TEST_CASE_P(ReferenceType, SharedFutureTests, future_tests_env&);

} // anonymous namespace
