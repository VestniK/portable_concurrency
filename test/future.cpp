#include <chrono>
#include <iostream>
#include <memory>
#include <string>
#include <thread>

#include <gtest/gtest.h>

#include "portable_concurrency/future"

#include "test_tools.h"
#include "test_helpers.h"

using namespace std::literals;
using sys_clock = std::chrono::system_clock;
using hires_clock = std::chrono::high_resolution_clock;

namespace {

template<typename T>
class FutureTests: public ::testing::Test {};
TYPED_TEST_CASE_P(FutureTests);

TYPED_TEST_P(FutureTests, default_constructed_is_invalid) {
  pc::future<TypeParam> future;
  EXPECT_FALSE(future.valid());
}

TYPED_TEST_P(FutureTests, obtained_from_promise_is_valid) {
  pc::promise<TypeParam> promise;
  auto future = promise.get_future();
  EXPECT_TRUE(future.valid());
}

TYPED_TEST_P(FutureTests, moved_to_constructor_is_invalid) {
  pc::promise<TypeParam> promise;
  auto future = promise.get_future();
  EXPECT_TRUE(future.valid());
  pc::future<TypeParam> another_future{std::move(future)};
  EXPECT_FALSE(future.valid());
  EXPECT_TRUE(another_future.valid());
}

TYPED_TEST_P(FutureTests, moved_to_assigment_to_invalid_is_invalid) {
  pc::promise<TypeParam> promise;
  auto future = promise.get_future();
  EXPECT_TRUE(future.valid());
  pc::future<TypeParam> another_future;
  EXPECT_FALSE(another_future.valid());
  another_future= std::move(future);
  EXPECT_FALSE(future.valid());
  EXPECT_TRUE(another_future.valid());
}

TYPED_TEST_P(FutureTests, moved_to_assigment_to_valid_is_invalid) {
  pc::promise<TypeParam> promise1;
  pc::promise<TypeParam> promise2;
  auto future1 = promise1.get_future();
  auto future2 = promise2.get_future();
  EXPECT_TRUE(future1.valid());
  EXPECT_TRUE(future2.valid());

  future2 = std::move(future1);
  EXPECT_TRUE(future2.valid());
  EXPECT_FALSE(future1.valid());
}

TYPED_TEST_P(FutureTests, share_invalidates_future) {
  pc::promise<TypeParam> promise;
  auto future = promise.get_future();
  ASSERT_TRUE(future.valid());
  auto shared = future.share();
  EXPECT_TRUE(shared.valid());
  EXPECT_FALSE(future.valid());
}

TYPED_TEST_P(FutureTests, is_ready_on_nonready) {
  pc::promise<TypeParam> promise;
  auto future = promise.get_future();
  EXPECT_FALSE(future.is_ready());
}

TYPED_TEST_P(FutureTests, is_ready_on_future_with_value) {
  pc::promise<TypeParam> promise;
  auto future = promise.get_future();
  set_promise_value<TypeParam>(promise);
  EXPECT_TRUE(future.is_ready());
}

TYPED_TEST_P(FutureTests, is_ready_on_future_with_error) {
  pc::promise<TypeParam> promise;
  auto future = promise.get_future();
  promise.set_exception(std::make_exception_ptr(std::runtime_error("test error")));
  EXPECT_TRUE(future.is_ready());
}

TYPED_TEST_P(FutureTests, get_on_invalid) {
  pc::future<TypeParam> future;
  ASSERT_FALSE(future.valid());
  EXPECT_FUTURE_ERROR(future.get(), std::future_errc::no_state);
}

template<typename T>
void test_retrieve_future_result() {
  auto future = set_value_in_other_thread<T>(25ms);
  ASSERT_TRUE(future.valid());

  EXPECT_EQ(some_value<T>(), future.get());
  EXPECT_FALSE(future.valid());
}

template<>
void test_retrieve_future_result<std::unique_ptr<int>>() {
  auto future = set_value_in_other_thread<std::unique_ptr<int>>(25ms);
  ASSERT_TRUE(future.valid());

  EXPECT_EQ(42, *future.get());
  EXPECT_FALSE(future.valid());
}

template<>
void test_retrieve_future_result<void>() {
  auto future = set_value_in_other_thread<void>(25ms);
  ASSERT_TRUE(future.valid());

  EXPECT_NO_THROW(future.get());
  EXPECT_FALSE(future.valid());
}

template<>
void test_retrieve_future_result<future_tests_env&>() {
  auto future = set_value_in_other_thread<future_tests_env&>(25ms);
  ASSERT_TRUE(future.valid());

  EXPECT_EQ(g_future_tests_env, &future.get());
  EXPECT_FALSE(future.valid());
}

TYPED_TEST_P(FutureTests, retrieve_result) {
  test_retrieve_future_result<TypeParam>();
}

TYPED_TEST_P(FutureTests, retrieve_exception) {
  auto future = set_error_in_other_thread<TypeParam>(25ms, std::runtime_error("test error"));
  ASSERT_TRUE(future.valid());

  EXPECT_RUNTIME_ERROR(future, "test error");
  EXPECT_FALSE(future.valid());
}

TYPED_TEST_P(FutureTests, wait_on_invalid) {
  pc::future<TypeParam> future;
  ASSERT_FALSE(future.valid());

  EXPECT_FUTURE_ERROR(future.wait(), std::future_errc::no_state);
}

TYPED_TEST_P(FutureTests, wait_for_on_invalid) {
  pc::future<TypeParam> future;
  ASSERT_FALSE(future.valid());

  EXPECT_FUTURE_ERROR(future.wait_for(5s), std::future_errc::no_state);
}

TYPED_TEST_P(FutureTests, wait_until_on_invalid) {
  pc::future<TypeParam> future;
  ASSERT_FALSE(future.valid());

  EXPECT_FUTURE_ERROR(
    future.wait_until(sys_clock::now() + 5s),
    std::future_errc::no_state
  );
}

TYPED_TEST_P(FutureTests, wait_on_ready_value) {
  pc::promise<TypeParam> promise;
  auto future = promise.get_future();
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

TYPED_TEST_P(FutureTests, wait_on_ready_error) {
  pc::promise<TypeParam> promise;
  auto future = promise.get_future();
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

TYPED_TEST_P(FutureTests, wait_timeout) {
  pc::promise<TypeParam> promise;
  auto future = promise.get_future();
  ASSERT_TRUE(future.valid());
  ASSERT_FALSE(future.is_ready());

  EXPECT_EQ(std::future_status::timeout, future.wait_for(5ms));
  EXPECT_TRUE(future.valid());
  EXPECT_FALSE(future.is_ready());

  EXPECT_EQ(std::future_status::timeout, future.wait_until(
    hires_clock::now() + 5ms
  ));
  EXPECT_TRUE(future.valid());
  EXPECT_FALSE(future.is_ready());
}

TYPED_TEST_P(FutureTests, wait_awakes_on_value) {
  auto future = set_value_in_other_thread<TypeParam>(25ms);
  ASSERT_TRUE(future.valid());
  ASSERT_FALSE(future.is_ready());

  EXPECT_NO_THROW(future.wait());
  EXPECT_TRUE(future.valid());
  EXPECT_TRUE(future.is_ready());
}

TYPED_TEST_P(FutureTests, wait_for_awakes_on_value) {
  auto future = set_value_in_other_thread<TypeParam>(25ms);
  ASSERT_TRUE(future.valid());
  ASSERT_FALSE(future.is_ready());

  EXPECT_NO_THROW(future.wait_for(15s));
  EXPECT_TRUE(future.valid());
  EXPECT_TRUE(future.is_ready());
}

TYPED_TEST_P(FutureTests, wait_until_awakes_on_value) {
  auto future = set_value_in_other_thread<TypeParam>(25ms);
  ASSERT_TRUE(future.valid());
  ASSERT_FALSE(future.is_ready());

  EXPECT_NO_THROW(future.wait_until(sys_clock::now() + 15s));
  EXPECT_TRUE(future.valid());
  EXPECT_TRUE(future.is_ready());
}

TYPED_TEST_P(FutureTests, wait_awakes_on_error) {
  auto future = set_error_in_other_thread<TypeParam>(25ms, std::runtime_error("test error"));
  ASSERT_TRUE(future.valid());
  ASSERT_FALSE(future.is_ready());

  EXPECT_NO_THROW(future.wait());
  EXPECT_TRUE(future.valid());
  EXPECT_TRUE(future.is_ready());
}

TYPED_TEST_P(FutureTests, wait_for_awakes_on_error) {
  auto future = set_error_in_other_thread<TypeParam>(25ms, std::runtime_error("test error"));
  ASSERT_TRUE(future.valid());
  ASSERT_FALSE(future.is_ready());

  EXPECT_NO_THROW(future.wait_for(15s));
  EXPECT_TRUE(future.valid());
  EXPECT_TRUE(future.is_ready());
}

TYPED_TEST_P(FutureTests, wait_until_awakes_on_error) {
  auto future = set_error_in_other_thread<TypeParam>(25ms, std::runtime_error("test error"));
  ASSERT_TRUE(future.valid());
  ASSERT_FALSE(future.is_ready());

  EXPECT_NO_THROW(future.wait_until(sys_clock::now() + 15s));
  EXPECT_TRUE(future.valid());
  EXPECT_TRUE(future.is_ready());
}

template<typename T>
void test_ready_future_maker() {static_assert(sizeof(T) == 0, "test_ready_future_maker<T> is deleted");} // = delete; in C++ but not in clang++

template<>
void test_ready_future_maker<int>() {
  auto future = pc::make_ready_future(42);
  ASSERT_TRUE(future.valid());
  EXPECT_TRUE(future.is_ready());
  EXPECT_EQ(42, future.get());
}

template<>
void test_ready_future_maker<std::string>() {
  auto future = pc::make_ready_future("hello"s);
  ASSERT_TRUE(future.valid());
  EXPECT_TRUE(future.is_ready());
  EXPECT_EQ("hello"s, future.get());
}

template<>
void test_ready_future_maker<std::unique_ptr<int>>() {
  auto future = pc::make_ready_future(std::make_unique<int>(42));
  ASSERT_TRUE(future.valid());
  EXPECT_TRUE(future.is_ready());
  EXPECT_EQ(42, *future.get());
}

template<>
void test_ready_future_maker<future_tests_env&>() {
  auto future = pc::make_ready_future(std::ref(*g_future_tests_env));
  static_assert(
    std::is_same<decltype(future), pc::future<future_tests_env&>>::value,
    "Incorrect future type"
  );
  ASSERT_TRUE(future.valid());
  EXPECT_TRUE(future.is_ready());
  EXPECT_EQ(g_future_tests_env, &future.get());
}

template<>
void test_ready_future_maker<void>() {
  auto future = pc::make_ready_future();
  ASSERT_TRUE(future.valid());
  EXPECT_TRUE(future.is_ready());
  EXPECT_NO_THROW(future.get());
}

TYPED_TEST_P(FutureTests, ready_future_maker) {
  test_ready_future_maker<TypeParam>();
}

TYPED_TEST_P(FutureTests, error_future_maker_from_exception_val) {
  auto future = pc::make_exceptional_future<TypeParam>(std::runtime_error("test error"));
  ASSERT_TRUE(future.valid());
  EXPECT_TRUE(future.is_ready());

  EXPECT_RUNTIME_ERROR(future, "test error");
  EXPECT_FALSE(future.valid());
}

TYPED_TEST_P(FutureTests, error_future_maker_from_caught_exception) {
  pc::future<TypeParam> future;
  try {
    throw std::runtime_error("test error");
  } catch(...) {
    future = pc::make_exceptional_future<TypeParam>(std::current_exception());
  }
  ASSERT_TRUE(future.valid());
  EXPECT_TRUE(future.is_ready());

  EXPECT_RUNTIME_ERROR(future, "test error");
  EXPECT_FALSE(future.valid());
}

REGISTER_TYPED_TEST_CASE_P(
  FutureTests,
  default_constructed_is_invalid,
  obtained_from_promise_is_valid,
  moved_to_constructor_is_invalid,
  moved_to_assigment_to_invalid_is_invalid,
  moved_to_assigment_to_valid_is_invalid,
  share_invalidates_future,
  is_ready_on_nonready,
  is_ready_on_future_with_value,
  is_ready_on_future_with_error,
  get_on_invalid,
  retrieve_result,
  retrieve_exception,
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
  wait_until_awakes_on_error,
  ready_future_maker,
  error_future_maker_from_exception_val,
  error_future_maker_from_caught_exception
);

INSTANTIATE_TYPED_TEST_CASE_P(VoidType, FutureTests, void);
INSTANTIATE_TYPED_TEST_CASE_P(PrimitiveType, FutureTests, int);
INSTANTIATE_TYPED_TEST_CASE_P(CopyableType, FutureTests, std::string);
INSTANTIATE_TYPED_TEST_CASE_P(MoveableType, FutureTests, std::unique_ptr<int>);
INSTANTIATE_TYPED_TEST_CASE_P(ReferenceType, FutureTests, future_tests_env&);

} // anonymous namespace
