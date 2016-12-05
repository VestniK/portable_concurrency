#include <chrono>
#include <iostream>
#include <thread>

#include <gtest/gtest.h>

#include "concurrency/future"

using namespace std::literals;

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
