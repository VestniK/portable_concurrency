#include <chrono>
#include <iostream>
#include <thread>

#include <gtest/gtest.h>

#include "concurrency/future"

using namespace std::literals;

TEST(FutureTests, default_cinstructed_is_invalid) {
  concurrency::future<int> future;
  EXPECT_FALSE(future.valid());
}

TEST(FutureTests, obtained_from_promise_is_valid) {
  concurrency::promise<int> promise;
  auto future = promise.get_future();
  EXPECT_TRUE(future.valid());
}

TEST(FutureTests, moved_to_constructor_is_invalid) {
  concurrency::promise<int> promise;
  auto future = promise.get_future();
  EXPECT_TRUE(future.valid());
  concurrency::future<int> another_future{std::move(future)};
  EXPECT_FALSE(future.valid());
  EXPECT_TRUE(another_future.valid());
}

TEST(FutureTests, moved_to_assigment_to_invalid_is_invalid) {
  concurrency::promise<int> promise;
  auto future = promise.get_future();
  EXPECT_TRUE(future.valid());
  concurrency::future<int> another_future;
  EXPECT_FALSE(another_future.valid());
  another_future= std::move(future);
  EXPECT_FALSE(future.valid());
  EXPECT_TRUE(another_future.valid());
}

TEST(FutureTests, moved_to_assigment_to_valid_is_invalid) {
  concurrency::promise<int> promise1;
  concurrency::promise<int> promise2;
  auto future1 = promise1.get_future();
  auto future2 = promise2.get_future();
  EXPECT_TRUE(future1.valid());
  EXPECT_TRUE(future2.valid());

  future2 = std::move(future1);
  EXPECT_TRUE(future2.valid());
  EXPECT_FALSE(future1.valid());
}

TEST(FutureTests, retrieve_result) {
  concurrency::promise<int> promise;
  auto future = promise.get_future();
  std::thread worker([promise = std::move(promise)]() mutable {
    std::this_thread::sleep_for(500ms);
    promise.set_value(42);
  });
  worker.detach();

  EXPECT_EQ(42, future.get());
}
