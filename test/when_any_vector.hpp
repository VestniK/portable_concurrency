#pragma once

#include <algorithm>

#include <gtest/gtest.h>

#include <concurrency/future>

namespace {

TEST(WhenAnyVectorTest, simple) {
  experimental::promise<int> ps[5];
  experimental::future<int> fs[5];

  std::transform(
    std::begin(ps), std::end(ps),
    std::begin(fs),
    std::mem_fn(&experimental::promise<int>::get_future)
  );

  auto f = experimental::when_any(std::begin(fs), std::end(fs));
  ASSERT_TRUE(f.valid());
  EXPECT_FALSE(f.is_ready());

  ps[3].set_value(42);
  EXPECT_TRUE(f.is_ready());

  auto res = f.get();
  EXPECT_EQ(res.index, 3u);
  std::size_t idx = 0;
  for (auto& fc: res.futures)
  {
    ASSERT_TRUE(fc.valid());
    EXPECT_EQ(fc.is_ready(), idx++ == 3u);
  }
  EXPECT_EQ(res.futures[3].get(), 42);
}

}

