#include <gtest/gtest.h>

#include "concurrency/future"

#include "test_tools.h"

TEST(PromiseTest, get_future_twice) {
  concurrency::promise<int> promise;
  concurrency::future<int> f1, f2;
  EXPECT_NO_THROW(f1 = promise.get_future());
  EXPECT_FUTURE_ERROR(f2 = promise.get_future(), concurrency::future_errc::future_already_retrieved);
}
