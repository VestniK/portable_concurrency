#include <algorithm>
#include <list>

#include <gtest/gtest.h>

#include <concurrency/future>

#include "test_helpers.h"

namespace {

TEST(WhenAnyVectorTest, empty_sequence) {
  std::list<experimental::future<int>> empty;
  auto f = experimental::when_any(empty.begin(), empty.end());

  ASSERT_TRUE(f.valid());
  ASSERT_TRUE(f.is_ready());

  auto res = f.get();
  EXPECT_EQ(res.index, static_cast<std::size_t>(-1));
  EXPECT_EQ(res.futures.size(), 0u);
}

TEST(WhenAnyVectorTest, single_future) {
  experimental::promise<std::string> p;
  auto raw_f = p.get_future();
  auto f = experimental::when_any(&raw_f, &raw_f + 1);
  ASSERT_TRUE(f.valid());
  EXPECT_FALSE(raw_f.valid());
  EXPECT_FALSE(f.is_ready());

  p.set_value("Hello");
  ASSERT_TRUE(f.is_ready());

  auto res = f.get();
  EXPECT_EQ(res.index, 0u);
  EXPECT_EQ(res.futures.size(), 1u);

  ASSERT_TRUE(res.futures[0].valid());
  ASSERT_TRUE(res.futures[0].is_ready());
  EXPECT_EQ(res.futures[0].get(), "Hello");
}

TEST(WhenAnyVectorTest, single_shared_future) {
  experimental::promise<std::unique_ptr<int>> p;
  auto raw_f = p.get_future().share();
  auto f = experimental::when_any(&raw_f, &raw_f + 1);
  ASSERT_TRUE(f.valid());
  EXPECT_TRUE(raw_f.valid());
  EXPECT_FALSE(f.is_ready());

  p.set_value(std::make_unique<int>(42));
  ASSERT_TRUE(f.is_ready());

  auto res = f.get();
  EXPECT_EQ(res.index, 0u);
  EXPECT_EQ(res.futures.size(), 1u);

  ASSERT_TRUE(res.futures[0].valid());
  ASSERT_TRUE(res.futures[0].is_ready());
  EXPECT_EQ(*res.futures[0].get(), 42);
}

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
  ASSERT_TRUE(f.is_ready());

  auto res = f.get();
  EXPECT_EQ(res.index, 3u);
  EXPECT_EQ(res.futures.size(), 5u);
  std::size_t idx = 0;
  for (auto& fc: res.futures)
  {
    ASSERT_TRUE(fc.valid());
    EXPECT_EQ(fc.is_ready(), idx++ == 3u);
  }
  EXPECT_EQ(res.futures[3].get(), 42);
}

}

