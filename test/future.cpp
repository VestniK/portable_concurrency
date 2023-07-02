#include <chrono>
#include <future>
#include <iostream>
#include <memory>
#include <string>
#include <thread>

#include <gtest/gtest.h>

#include <portable_concurrency/future>

#include "test_helpers.h"
#include "test_tools.h"

using namespace std::literals;
using sys_clock = std::chrono::system_clock;
using hires_clock = std::chrono::high_resolution_clock;

namespace {

struct future : ::testing::Test {};

TEST_F(future, default_constructed_is_invalid) {
  pc::future<void> future;
  EXPECT_FALSE(future.valid());
}

TEST_F(future, obtained_from_promise_is_valid) {
  pc::promise<int> p;
  pc::future<int> future = p.get_future();
  EXPECT_TRUE(future.valid());
}

TEST_F(future, moved_to_constructor_is_invalid) {
  pc::promise<std::string> p;
  pc::future<std::string> future = p.get_future();
  pc::future<std::string> another_future{std::move(future)};
  EXPECT_FALSE(future.valid());
}

TEST_F(future, move_constructed_from_valid_is_valid) {
  pc::promise<std::string &> p;
  pc::future<std::string &> future = p.get_future();
  pc::future<std::string &> another_future{std::move(future)};
  EXPECT_TRUE(another_future.valid());
}

TEST_F(future, moved_to_assigment_to_invalid_is_invalid) {
  pc::promise<std::unique_ptr<int>> p;
  pc::future<std::unique_ptr<int>> src_future = p.get_future();
  pc::future<std::unique_ptr<int>> dst_future;
  dst_future = std::move(src_future);
  EXPECT_FALSE(src_future.valid());
}

TEST_F(future, invalid_move_assigned_from_valid_is_valid) {
  pc::promise<std::unique_ptr<int>> p;
  pc::future<std::unique_ptr<int>> src_future = p.get_future();
  pc::future<std::unique_ptr<int>> dst_future;
  dst_future = std::move(src_future);
  EXPECT_TRUE(dst_future.valid());
}

TEST_F(future, moved_to_assigment_to_valid_is_invalid) {
  pc::promise<int &> p[2];
  pc::future<int &> future[] = {p[0].get_future(), p[1].get_future()};

  future[1] = std::move(future[0]);
  EXPECT_TRUE(future[1].valid());
}

TEST_F(future, valid_move_assigned_with_another_valid_is_valid) {
  pc::promise<int &> p[2];
  pc::future<int &> future[] = {p[0].get_future(), p[1].get_future()};

  future[1] = std::move(future[0]);
  EXPECT_FALSE(future[0].valid());
}

TEST_F(future, share_invalidates_future) {
  pc::promise<std::string> p;
  pc::future<std::string> future = p.get_future();
  pc::shared_future<std::string> shared = future.share();
  EXPECT_FALSE(future.valid());
}

TEST_F(future, share_returns_valid_shared_future) {
  pc::promise<std::string> p;
  pc::future<std::string> future = p.get_future();
  pc::shared_future<std::string> shared = future.share();
  EXPECT_TRUE(shared.valid());
}

TEST_F(future, is_ready_return_false_on_nonready) {
  pc::promise<void> p;
  pc::future<void> future = p.get_future();
  EXPECT_FALSE(future.is_ready());
}

template <typename T> struct FutureTests : ::testing::Test {
  pc::promise<T> promise[2];
};
TYPED_TEST_CASE(FutureTests, TestTypes);

TYPED_TEST(FutureTests, is_ready_on_future_with_value) {
  auto future = this->promise[0].get_future();
  set_promise_value<TypeParam>(this->promise[0]);
  EXPECT_TRUE(future.is_ready());
}

TYPED_TEST(FutureTests, is_ready_on_future_with_error) {
  auto future = this->promise[0].get_future();
  this->promise[0].set_exception(
      std::make_exception_ptr(std::runtime_error("test error")));
  EXPECT_TRUE(future.is_ready());
}

TYPED_TEST(FutureTests, get_on_invalid) {
  pc::future<TypeParam> future;
  EXPECT_FUTURE_ERROR(future.get(), std::future_errc::no_state);
}

template <typename T> void test_retrieve_future_result() {
  auto future = set_value_in_other_thread<T>(25ms);
  ASSERT_TRUE(future.valid());

  EXPECT_EQ(some_value<T>(), future.get());
  EXPECT_FALSE(future.valid());
}

template <> void test_retrieve_future_result<std::unique_ptr<int>>() {
  auto future = set_value_in_other_thread<std::unique_ptr<int>>(25ms);
  ASSERT_TRUE(future.valid());

  EXPECT_EQ(42, *future.get());
  EXPECT_FALSE(future.valid());
}

template <> void test_retrieve_future_result<void>() {
  auto future = set_value_in_other_thread<void>(25ms);
  ASSERT_TRUE(future.valid());

  EXPECT_NO_THROW(future.get());
  EXPECT_FALSE(future.valid());
}

template <> void test_retrieve_future_result<future_tests_env &>() {
  auto future = set_value_in_other_thread<future_tests_env &>(25ms);
  ASSERT_TRUE(future.valid());

  EXPECT_EQ(g_future_tests_env, &future.get());
  EXPECT_FALSE(future.valid());
}

TYPED_TEST(FutureTests, retrieve_result) {
  test_retrieve_future_result<TypeParam>();
}

TYPED_TEST(FutureTests, retrieve_exception) {
  auto future = set_error_in_other_thread<TypeParam>(
      25ms, std::runtime_error("test error"));
  ASSERT_TRUE(future.valid());

  EXPECT_RUNTIME_ERROR(future, "test error");
  EXPECT_FALSE(future.valid());
}

TYPED_TEST(FutureTests, wait_on_invalid) {
  pc::future<TypeParam> future;
  ASSERT_FALSE(future.valid());

  EXPECT_FUTURE_ERROR(future.wait(), std::future_errc::no_state);
}

#if !defined(PC_NO_DEPRECATED)
TYPED_TEST(FutureTests, wait_for_on_invalid) {
  pc::future<TypeParam> future;
  ASSERT_FALSE(future.valid());

  EXPECT_FUTURE_ERROR(future.wait_for(5s), std::future_errc::no_state);
}

TYPED_TEST(FutureTests, wait_until_on_invalid) {
  pc::future<TypeParam> future;
  ASSERT_FALSE(future.valid());

  EXPECT_FUTURE_ERROR(future.wait_until(sys_clock::now() + 5s),
                      std::future_errc::no_state);
}
#endif

TYPED_TEST(FutureTests, wait_on_ready_value) {
  auto future = this->promise[0].get_future();
  set_promise_value<TypeParam>(this->promise[0]);
  ASSERT_TRUE(future.valid());
  ASSERT_TRUE(future.is_ready());

  EXPECT_NO_THROW(future.wait());
  EXPECT_TRUE(future.valid());
  EXPECT_TRUE(future.is_ready());

#if !defined(PC_NO_DEPRECATED)
  EXPECT_EQ(pc::future_status::ready, future.wait_for(5s));
  EXPECT_TRUE(future.valid());
  EXPECT_TRUE(future.is_ready());

  EXPECT_EQ(pc::future_status::ready, future.wait_until(sys_clock::now() + 5s));
  EXPECT_TRUE(future.valid());
  EXPECT_TRUE(future.is_ready());
#endif
}

TYPED_TEST(FutureTests, wait_on_ready_error) {
  auto future = this->promise[0].get_future();
  this->promise[0].set_exception(
      std::make_exception_ptr(std::runtime_error("test error")));
  ASSERT_TRUE(future.valid());
  ASSERT_TRUE(future.is_ready());

  EXPECT_NO_THROW(future.wait());
  EXPECT_TRUE(future.valid());
  EXPECT_TRUE(future.is_ready());

#if !defined(PC_NO_DEPRECATED)
  EXPECT_EQ(pc::future_status::ready, future.wait_for(5s));
  EXPECT_TRUE(future.valid());
  EXPECT_TRUE(future.is_ready());

  EXPECT_EQ(pc::future_status::ready, future.wait_until(sys_clock::now() + 5s));
  EXPECT_TRUE(future.valid());
  EXPECT_TRUE(future.is_ready());
#endif
}

#if !defined(PC_NO_DEPRECATED)
TYPED_TEST(FutureTests, wait_timeout) {
  auto future = this->promise[0].get_future();
  ASSERT_TRUE(future.valid());
  ASSERT_FALSE(future.is_ready());

  EXPECT_EQ(pc::future_status::timeout, future.wait_for(5ms));
  EXPECT_TRUE(future.valid());
  EXPECT_FALSE(future.is_ready());

  EXPECT_EQ(pc::future_status::timeout,
            future.wait_until(hires_clock::now() + 5ms));
  EXPECT_TRUE(future.valid());
  EXPECT_FALSE(future.is_ready());
}
#endif

TYPED_TEST(FutureTests, wait_awakes_on_value) {
  auto future = set_value_in_other_thread<TypeParam>(25ms);
  ASSERT_TRUE(future.valid());
  ASSERT_FALSE(future.is_ready());

  EXPECT_NO_THROW(future.wait());
  EXPECT_TRUE(future.valid());
  EXPECT_TRUE(future.is_ready());
}

#if !defined(PC_NO_DEPRECATED)
TYPED_TEST(FutureTests, wait_for_awakes_on_value) {
  auto future = set_value_in_other_thread<TypeParam>(25ms);
  ASSERT_TRUE(future.valid());
  ASSERT_FALSE(future.is_ready());

  EXPECT_NO_THROW(future.wait_for(15s));
  EXPECT_TRUE(future.valid());
  EXPECT_TRUE(future.is_ready());
}

TYPED_TEST(FutureTests, wait_until_awakes_on_value) {
  auto future = set_value_in_other_thread<TypeParam>(25ms);
  ASSERT_TRUE(future.valid());
  ASSERT_FALSE(future.is_ready());

  EXPECT_NO_THROW(future.wait_until(sys_clock::now() + 15s));
  EXPECT_TRUE(future.valid());
  EXPECT_TRUE(future.is_ready());
}
#endif

TYPED_TEST(FutureTests, wait_awakes_on_error) {
  auto future = set_error_in_other_thread<TypeParam>(
      25ms, std::runtime_error("test error"));
  ASSERT_TRUE(future.valid());
  ASSERT_FALSE(future.is_ready());

  EXPECT_NO_THROW(future.wait());
  EXPECT_TRUE(future.valid());
  EXPECT_TRUE(future.is_ready());
}

#if !defined(PC_NO_DEPRECATED)
TYPED_TEST(FutureTests, wait_for_awakes_on_error) {
  auto future = set_error_in_other_thread<TypeParam>(
      25ms, std::runtime_error("test error"));
  ASSERT_TRUE(future.valid());
  ASSERT_FALSE(future.is_ready());

  EXPECT_NO_THROW(future.wait_for(15s));
  EXPECT_TRUE(future.valid());
  EXPECT_TRUE(future.is_ready());
}
#endif

#if !defined(PC_NO_DEPRECATED)
TYPED_TEST(FutureTests, wait_until_awakes_on_error) {
  auto future = set_error_in_other_thread<TypeParam>(
      25ms, std::runtime_error("test error"));
  ASSERT_TRUE(future.valid());
  ASSERT_FALSE(future.is_ready());

  EXPECT_NO_THROW(future.wait_until(sys_clock::now() + 15s));
  EXPECT_TRUE(future.valid());
  EXPECT_TRUE(future.is_ready());
}
#endif

template <typename T> void test_ready_future_maker() {
  static_assert(sizeof(T) == 0, "test_ready_future_maker<T> is deleted");
} // = delete; in C++ but not in clang++

template <> void test_ready_future_maker<int>() {
  auto future = pc::make_ready_future(42);
  ASSERT_TRUE(future.valid());
  EXPECT_TRUE(future.is_ready());
  EXPECT_EQ(42, future.get());
}

template <> void test_ready_future_maker<std::string>() {
  auto future = pc::make_ready_future(std::string{"hello"});
  ASSERT_TRUE(future.valid());
  EXPECT_TRUE(future.is_ready());
  EXPECT_EQ(future.get(), "hello");
}

template <> void test_ready_future_maker<std::unique_ptr<int>>() {
  auto future = pc::make_ready_future(std::make_unique<int>(42));
  ASSERT_TRUE(future.valid());
  EXPECT_TRUE(future.is_ready());
  EXPECT_EQ(42, *future.get());
}

template <> void test_ready_future_maker<future_tests_env &>() {
  auto future = pc::make_ready_future(std::ref(*g_future_tests_env));
  static_assert(
      std::is_same<decltype(future), pc::future<future_tests_env &>>::value,
      "Incorrect future type");
  ASSERT_TRUE(future.valid());
  EXPECT_TRUE(future.is_ready());
  EXPECT_EQ(g_future_tests_env, &future.get());
}

template <> void test_ready_future_maker<void>() {
  auto future = pc::make_ready_future();
  ASSERT_TRUE(future.valid());
  EXPECT_TRUE(future.is_ready());
  EXPECT_NO_THROW(future.get());
}

TYPED_TEST(FutureTests, ready_future_maker) {
  test_ready_future_maker<TypeParam>();
}

TYPED_TEST(FutureTests, error_future_maker_from_exception_val) {
  auto future =
      pc::make_exceptional_future<TypeParam>(std::runtime_error("test error"));
  ASSERT_TRUE(future.valid());
  EXPECT_TRUE(future.is_ready());

  EXPECT_RUNTIME_ERROR(future, "test error");
  EXPECT_FALSE(future.valid());
}

TYPED_TEST(FutureTests, error_future_maker_from_caught_exception) {
  pc::future<TypeParam> future;
  try {
    throw std::runtime_error("test error");
  } catch (...) {
    future = pc::make_exceptional_future<TypeParam>(std::current_exception());
  }
  ASSERT_TRUE(future.valid());
  EXPECT_TRUE(future.is_ready());

  EXPECT_RUNTIME_ERROR(future, "test error");
  EXPECT_FALSE(future.valid());
}

} // anonymous namespace
