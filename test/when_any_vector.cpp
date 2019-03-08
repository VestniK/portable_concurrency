#include <algorithm>
#include <list>

#include <gtest/gtest.h>

#include <portable_concurrency/future>
#include <portable_concurrency/latch>

#include "test_helpers.h"

namespace {

TEST(WhenAnyVectorTest, empty_sequence) {
  std::list<pc::future<int>> empty;
  auto f = pc::when_any(empty.begin(), empty.end());

  ASSERT_TRUE(f.valid());
  ASSERT_TRUE(f.is_ready());

  auto res = f.get();
  EXPECT_EQ(res.index, static_cast<std::size_t>(-1));
  EXPECT_EQ(res.futures.size(), 0u);
}

TEST(WhenAnyVectorTest, single_future) {
  pc::promise<std::string> p;
  auto raw_f = p.get_future();
  auto f = pc::when_any(&raw_f, &raw_f + 1);
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
  pc::promise<std::unique_ptr<int>> p;
  auto raw_f = p.get_future().share();
  auto f = pc::when_any(&raw_f, &raw_f + 1);
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

TEST(WhenAnyVectorTest, single_ready_future) {
  auto raw_f = pc::make_ready_future(123);
  auto f = pc::when_any(&raw_f, &raw_f + 1);
  ASSERT_TRUE(f.valid());
  EXPECT_FALSE(raw_f.valid());
  ASSERT_TRUE(f.is_ready());

  auto res = f.get();
  EXPECT_EQ(res.index, 0u);
  EXPECT_EQ(res.futures.size(), 1u);

  ASSERT_TRUE(res.futures[0].valid());
  ASSERT_TRUE(res.futures[0].is_ready());
  EXPECT_EQ(res.futures[0].get(), 123);
}

TEST(WhenAnyVectorTest, single_ready_shared_future) {
  auto raw_f = pc::make_ready_future(123).share();
  auto f = pc::when_any(&raw_f, &raw_f + 1);
  ASSERT_TRUE(f.valid());
  EXPECT_TRUE(raw_f.valid());
  ASSERT_TRUE(f.is_ready());

  auto res = f.get();
  EXPECT_EQ(res.index, 0u);
  EXPECT_EQ(res.futures.size(), 1u);

  ASSERT_TRUE(res.futures[0].valid());
  ASSERT_TRUE(res.futures[0].is_ready());
  EXPECT_EQ(res.futures[0].get(), 123);
}

TEST(WhenAnyVectorTest, single_error_future) {
  auto raw_f = pc::make_exceptional_future<future_tests_env&>(std::runtime_error("panic"));
  auto f = pc::when_any(&raw_f, &raw_f + 1);
  ASSERT_TRUE(f.valid());
  EXPECT_FALSE(raw_f.valid());
  ASSERT_TRUE(f.is_ready());

  auto res = f.get();
  EXPECT_EQ(res.index, 0u);
  EXPECT_EQ(res.futures.size(), 1u);

  ASSERT_TRUE(res.futures[0].valid());
  ASSERT_TRUE(res.futures[0].is_ready());
  EXPECT_RUNTIME_ERROR(res.futures[0], "panic");
}

TEST(WhenAnyVectorTest, single_error_shared_future) {
  auto raw_f = pc::make_exceptional_future<future_tests_env&>(std::runtime_error("panic")).share();
  auto f = pc::when_any(&raw_f, &raw_f + 1);
  ASSERT_TRUE(f.valid());
  EXPECT_TRUE(raw_f.valid());
  ASSERT_TRUE(f.is_ready());

  auto res = f.get();
  EXPECT_EQ(res.index, 0u);
  EXPECT_EQ(res.futures.size(), 1u);

  ASSERT_TRUE(res.futures[0].valid());
  ASSERT_TRUE(res.futures[0].is_ready());
  EXPECT_RUNTIME_ERROR(res.futures[0], "panic");
}

TEST(WhenAnyVectorTest, multiple_futures) {
  pc::promise<int> ps[5];
  pc::future<int> fs[5];

  std::transform(std::begin(ps), std::end(ps), std::begin(fs), get_promise_future);

  auto f = pc::when_any(std::begin(fs), std::end(fs));
  ASSERT_TRUE(f.valid());
  EXPECT_FALSE(f.is_ready());
  for (const auto& fi : fs)
    EXPECT_FALSE(fi.valid());

  ps[3].set_value(42);
  ASSERT_TRUE(f.is_ready());

  auto res = f.get();
  EXPECT_EQ(res.index, 3u);
  EXPECT_EQ(res.futures.size(), 5u);
  std::size_t idx = 0;
  for (auto& fc : res.futures) {
    ASSERT_TRUE(fc.valid());
    EXPECT_EQ(fc.is_ready(), idx++ == res.index);
  }
  EXPECT_EQ(res.futures[res.index].get(), 42);
}

TEST(WhenAnyVectorTest, multiple_shared_futures) {
  pc::promise<std::string> ps[5];
  pc::shared_future<std::string> fs[5];

  std::transform(std::begin(ps), std::end(ps), std::begin(fs), get_promise_future);

  auto f = pc::when_any(std::begin(fs), std::end(fs));
  ASSERT_TRUE(f.valid());
  EXPECT_FALSE(f.is_ready());
  for (const auto& fi : fs)
    EXPECT_TRUE(fi.valid());

  ps[2].set_value("42");
  ASSERT_TRUE(f.is_ready());

  auto res = f.get();
  EXPECT_EQ(res.index, 2u);
  EXPECT_EQ(res.futures.size(), 5u);
  std::size_t idx = 0;
  for (auto& fc : res.futures) {
    ASSERT_TRUE(fc.valid());
    EXPECT_EQ(fc.is_ready(), idx++ == res.index);
  }
  EXPECT_EQ(res.futures[res.index].get(), "42");
}

TEST(WhenAnyVectorTest, multiple_futures_one_initionally_ready) {
  pc::promise<void> ps[5];
  pc::future<void> fs[5];

  std::transform(std::begin(ps), std::end(ps), std::begin(fs), get_promise_future);
  ps[1].set_value();
  ASSERT_TRUE(fs[1].is_ready());

  auto f = pc::when_any(std::begin(fs), std::end(fs));
  ASSERT_TRUE(f.valid());
  ASSERT_TRUE(f.is_ready());

  auto res = f.get();
  EXPECT_EQ(res.index, 1u);
  EXPECT_EQ(res.futures.size(), 5u);
  std::size_t idx = 0;
  for (auto& fc : res.futures) {
    ASSERT_TRUE(fc.valid());
    EXPECT_EQ(fc.is_ready(), idx++ == res.index);
  }
  ASSERT_NO_THROW(res.futures[res.index].get());
}

TEST(WhenAnyVectorTest, multiple_shared_futures_one_initionally_ready) {
  int some_var = 42;

  pc::promise<int&> ps[5];
  pc::shared_future<int&> fs[5];

  std::transform(std::begin(ps), std::end(ps), std::begin(fs), get_promise_future);
  ps[0].set_value(some_var);
  ASSERT_TRUE(fs[0].is_ready());

  auto f = pc::when_any(std::begin(fs), std::end(fs));
  ASSERT_TRUE(f.valid());
  ASSERT_TRUE(f.is_ready());

  auto res = f.get();
  EXPECT_EQ(res.index, 0u);
  EXPECT_EQ(res.futures.size(), 5u);
  std::size_t idx = 0;
  for (auto& fc : res.futures) {
    ASSERT_TRUE(fc.valid());
    EXPECT_EQ(fc.is_ready(), idx++ == res.index);
  }
  EXPECT_EQ(&res.futures[res.index].get(), &some_var);
}

TEST(WhenAnyVectorTest, multiple_futures_one_initionally_error) {
  pc::promise<std::unique_ptr<int>> ps[5];
  pc::future<std::unique_ptr<int>> fs[5];

  std::transform(std::begin(ps), std::end(ps), std::begin(fs), get_promise_future);
  ps[4].set_exception(std::make_exception_ptr(std::runtime_error("epic fail")));
  ASSERT_TRUE(fs[4].is_ready());

  auto f = pc::when_any(std::begin(fs), std::end(fs));
  ASSERT_TRUE(f.valid());
  ASSERT_TRUE(f.is_ready());

  auto res = f.get();
  EXPECT_EQ(res.index, 4u);
  EXPECT_EQ(res.futures.size(), 5u);
  std::size_t idx = 0;
  for (auto& fc : res.futures) {
    ASSERT_TRUE(fc.valid());
    EXPECT_EQ(fc.is_ready(), idx++ == res.index);
  }
  EXPECT_RUNTIME_ERROR(res.futures[res.index], "epic fail");
}

TEST(WhenAnyVectorTest, multiple_shared_futures_one_initionally_error) {
  pc::promise<int> ps[5];
  pc::shared_future<int> fs[5];

  std::transform(std::begin(ps), std::end(ps), std::begin(fs), get_promise_future);
  ps[4].set_exception(std::make_exception_ptr(std::runtime_error("epic fail")));
  ASSERT_TRUE(fs[4].is_ready());

  auto f = pc::when_any(std::begin(fs), std::end(fs));
  ASSERT_TRUE(f.valid());
  ASSERT_TRUE(f.is_ready());

  auto res = f.get();
  EXPECT_EQ(res.index, 4u);
  EXPECT_EQ(res.futures.size(), 5u);
  std::size_t idx = 0;
  for (auto& fc : res.futures) {
    ASSERT_TRUE(fc.valid());
    EXPECT_EQ(fc.is_ready(), idx++ == res.index);
  }
  EXPECT_RUNTIME_ERROR(res.futures[res.index], "epic fail");
}

TEST(WhenAnyVectorTest, next_ready_futures_dont_affect_result_before_get) {
  pc::promise<size_t> ps[5];
  pc::shared_future<size_t> fs[5];

  std::transform(std::begin(ps), std::end(ps), std::begin(fs), get_promise_future);

  auto f = pc::when_any(std::begin(fs), std::end(fs));
  ASSERT_TRUE(f.valid());
  EXPECT_FALSE(f.is_ready());

  for (size_t pos = 0; pos < 5; ++pos) {
    ps[(pos + 3) % 5].set_value(pos + 3);
    ASSERT_TRUE(f.is_ready());
  }
  auto res = f.get();
  EXPECT_EQ(res.index, 3u);
}

TEST(WhenAnyVectorTest, next_ready_futures_dont_affect_result_after_get) {
  pc::promise<size_t> ps[5];
  pc::shared_future<size_t> fs[5];

  std::transform(std::begin(ps), std::end(ps), std::begin(fs), get_promise_future);

  auto f = pc::when_any(std::begin(fs), std::end(fs));
  ASSERT_TRUE(f.valid());
  EXPECT_FALSE(f.is_ready());

  ps[2].set_value(42);
  ASSERT_TRUE(f.is_ready());
  auto res = f.get();
  EXPECT_EQ(res.index, 2u);

  for (size_t pos = 0; pos < 4; ++pos) {
    ps[(pos + 3) % 5].set_value(pos + 100500);
    EXPECT_EQ(res.index, 2u);
  }
}

TEST(WhenAnyVectorTest, futures_becomes_ready_concurrently) {
  pc::promise<size_t> ps[3];
  pc::shared_future<size_t> fs[3];

  std::transform(std::begin(ps), std::end(ps), std::begin(fs), get_promise_future);
  auto f = pc::when_any(std::begin(fs), std::end(fs));
  ASSERT_TRUE(f.valid());
  EXPECT_FALSE(f.is_ready());

  pc::latch latch{3};
  g_future_tests_env->run_async([&latch, p = std::move(ps[1])]() mutable {
    latch.count_down_and_wait();
    p.set_value(123);
  });
  g_future_tests_env->run_async([&latch, p = std::move(ps[2])]() mutable {
    latch.count_down_and_wait();
    p.set_value(345);
  });

  latch.count_down_and_wait();
  auto res = f.get();
  EXPECT_TRUE(res.index == 1 || res.index == 2) << "unexpected index: " << res.index;
}

} // anonymous namespace
