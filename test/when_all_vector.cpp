#include <algorithm>
#include <list>

#include <gtest/gtest.h>

#include <concurrency/future>
#include <concurrency/latch>

#include "test_helpers.h"

namespace {

TEST(WhenAllVectorTest, empty_sequence) {
  std::list<experimental::future<int>> empty;
  auto f = experimental::when_all(empty.begin(), empty.end());

  ASSERT_TRUE(f.valid());
  ASSERT_TRUE(f.is_ready());

  std::vector<experimental::future<int>> res = f.get();
  EXPECT_EQ(res.size(), 0u);
}

TEST(WhenAllVectorTest, single_future) {
  experimental::promise<std::string> p;
  auto raw_f = p.get_future();
  auto f = experimental::when_all(&raw_f, &raw_f + 1);
  ASSERT_TRUE(f.valid());
  EXPECT_FALSE(raw_f.valid());
  EXPECT_FALSE(f.is_ready());

  p.set_value("Hello");
  ASSERT_TRUE(f.is_ready());

  auto res = f.get();
  EXPECT_EQ(res.size(), 1u);

  ASSERT_TRUE(res[0].valid());
  ASSERT_TRUE(res[0].is_ready());
  EXPECT_EQ(res[0].get(), "Hello");
}

TEST(WhenAllVectorTest, single_shared_future) {
  experimental::promise<std::unique_ptr<int>> p;
  auto raw_f = p.get_future().share();
  auto f = experimental::when_all(&raw_f, &raw_f + 1);
  ASSERT_TRUE(f.valid());
  EXPECT_TRUE(raw_f.valid());
  EXPECT_FALSE(f.is_ready());

  p.set_value(std::make_unique<int>(42));
  ASSERT_TRUE(f.is_ready());

  auto res = f.get();
  EXPECT_EQ(res.size(), 1u);

  ASSERT_TRUE(res[0].valid());
  ASSERT_TRUE(res[0].is_ready());
  EXPECT_EQ(*res[0].get(), 42);
}

TEST(WhenAllVectorTest, single_ready_future) {
  auto raw_f = experimental::make_ready_future(123);
  auto f = experimental::when_all(&raw_f, &raw_f + 1);
  ASSERT_TRUE(f.valid());
  EXPECT_FALSE(raw_f.valid());
  ASSERT_TRUE(f.is_ready());

  auto res = f.get();
  EXPECT_EQ(res.size(), 1u);

  ASSERT_TRUE(res[0].valid());
  ASSERT_TRUE(res[0].is_ready());
  EXPECT_EQ(res[0].get(), 123);
}

TEST(WhenAllVectorTest, single_ready_shared_future) {
  auto raw_f = experimental::make_ready_future(123).share();
  auto f = experimental::when_all(&raw_f, &raw_f + 1);
  ASSERT_TRUE(f.valid());
  EXPECT_TRUE(raw_f.valid());
  ASSERT_TRUE(f.is_ready());

  auto res = f.get();
  EXPECT_EQ(res.size(), 1u);

  ASSERT_TRUE(res[0].valid());
  ASSERT_TRUE(res[0].is_ready());
  EXPECT_EQ(res[0].get(), 123);
}

TEST(WhenAllVectorTest, single_error_future) {
  auto raw_f = experimental::make_exceptional_future<future_tests_env&>(
    std::runtime_error("panic")
  );
  auto f = experimental::when_all(&raw_f, &raw_f + 1);
  ASSERT_TRUE(f.valid());
  EXPECT_FALSE(raw_f.valid());
  ASSERT_TRUE(f.is_ready());

  auto res = f.get();
  EXPECT_EQ(res.size(), 1u);

  ASSERT_TRUE(res[0].valid());
  ASSERT_TRUE(res[0].is_ready());
  EXPECT_RUNTIME_ERROR(res[0], "panic");
}

TEST(WhenAllVectorTest, single_error_shared_future) {
  auto raw_f = experimental::make_exceptional_future<future_tests_env&>(
    std::runtime_error("panic")
  ).share();
  auto f = experimental::when_all(&raw_f, &raw_f + 1);
  ASSERT_TRUE(f.valid());
  EXPECT_TRUE(raw_f.valid());
  ASSERT_TRUE(f.is_ready());

  auto res = f.get();
  EXPECT_EQ(res.size(), 1u);

  ASSERT_TRUE(res[0].valid());
  ASSERT_TRUE(res[0].is_ready());
  EXPECT_RUNTIME_ERROR(res[0], "panic");
}

TEST(WhenAllVectorTest, multiple_futures) {
  experimental::promise<int> ps[5];
  experimental::future<int> fs[5];

  std::transform(
    std::begin(ps), std::end(ps),
    std::begin(fs),
    std::mem_fn(&experimental::promise<int>::get_future)
  );

  auto f = experimental::when_all(std::begin(fs), std::end(fs));
  ASSERT_TRUE(f.valid());
  EXPECT_FALSE(f.is_ready());
  for (const auto& fi: fs)
    EXPECT_FALSE(fi.valid());

  for (std::size_t pos: {3, 0, 1, 4, 2}) {
    EXPECT_FALSE(f.is_ready());
    ps[pos].set_value(42*pos);
  }
  ASSERT_TRUE(f.is_ready());

  auto res = f.get();
  EXPECT_EQ(res.size(), 5u);
  int idx = 0;
  for (auto& fc: res) {
    ASSERT_TRUE(fc.valid());
    ASSERT_TRUE(fc.is_ready());
    EXPECT_EQ(fc.get(), 42*idx++);
  }
}

TEST(WhenAllVectorTest, multiple_shared_futures) {
  experimental::promise<std::string> ps[5];
  experimental::shared_future<std::string> fs[5];

  std::transform(
    std::begin(ps), std::end(ps),
    std::begin(fs),
    std::mem_fn(&experimental::promise<std::string>::get_future)
  );

  auto f = experimental::when_all(std::begin(fs), std::end(fs));
  ASSERT_TRUE(f.valid());
  EXPECT_FALSE(f.is_ready());
  for (const auto& fi: fs)
    EXPECT_TRUE(fi.valid());

  for (std::size_t pos: {3, 0, 1, 4, 2}) {
    EXPECT_FALSE(f.is_ready());
    ps[pos].set_value(std::to_string(42*pos));
  }
  ASSERT_TRUE(f.is_ready());

  auto res = f.get();
  EXPECT_EQ(res.size(), 5u);
  std::size_t idx = 0;
  for (auto& fc: res) {
    ASSERT_TRUE(fc.valid());
    ASSERT_TRUE(fc.is_ready());
    EXPECT_EQ(fc.get(), std::to_string(42*idx++));
  }
}

TEST(WhenAllVectorTest, multiple_futures_one_initionally_ready) {
  experimental::promise<void> ps[5];
  experimental::future<void> fs[5];

  std::transform(
    std::begin(ps), std::end(ps),
    std::begin(fs),
    std::mem_fn(&experimental::promise<void>::get_future)
  );
  ps[1].set_value();
  ASSERT_TRUE(fs[1].is_ready());

  auto f = experimental::when_all(std::begin(fs), std::end(fs));
  ASSERT_TRUE(f.valid());

  for(std::size_t pos: {4, 0, 3, 2}) {
    EXPECT_FALSE(f.is_ready());
    ps[pos].set_value();
  }
  ASSERT_TRUE(f.is_ready());

  auto res = f.get();
  EXPECT_EQ(res.size(), 5u);
  for (auto& fc: res) {
    ASSERT_TRUE(fc.valid());
    ASSERT_TRUE(fc.is_ready());
    ASSERT_NO_THROW(fc.get());
  }
}

TEST(WhenAllVectorTest, multiple_shared_futures_one_initionally_ready) {
  int some_vars[5] = {5, 4, 3, 2, 1};
  experimental::promise<int&> ps[5];
  experimental::shared_future<int&> fs[5];

  std::transform(
    std::begin(ps), std::end(ps),
    std::begin(fs),
    std::mem_fn(&experimental::promise<int&>::get_future)
  );
  ps[0].set_value(some_vars[0]);
  ASSERT_TRUE(fs[0].is_ready());

  auto f = experimental::when_all(std::begin(fs), std::end(fs));
  ASSERT_TRUE(f.valid());
  for(std::size_t pos: {4, 1, 3, 2}) {
    EXPECT_FALSE(f.is_ready());
    ps[pos].set_value(some_vars[pos]);
  }
  ASSERT_TRUE(f.is_ready());

  auto res = f.get();
  EXPECT_EQ(res.size(), 5u);
  std::size_t idx = 0;
  for (auto& fc: res) {
    ASSERT_TRUE(fc.valid());
    ASSERT_TRUE(fc.is_ready());
    EXPECT_EQ(&fc.get(), &some_vars[idx++]);
  }
}

TEST(WhenAllVectorTest, multiple_futures_one_initionally_error) {
  experimental::promise<std::unique_ptr<int>> ps[5];
  experimental::future<std::unique_ptr<int>> fs[5];

  std::transform(
    std::begin(ps), std::end(ps),
    std::begin(fs),
    std::mem_fn(&experimental::promise<std::unique_ptr<int>>::get_future)
  );
  ps[4].set_exception(std::make_exception_ptr(std::runtime_error("epic fail")));
  ASSERT_TRUE(fs[4].is_ready());

  auto f = experimental::when_all(std::begin(fs), std::end(fs));
  ASSERT_TRUE(f.valid());

  for (std::size_t pos: {0, 1, 3, 2}) {
    EXPECT_FALSE(f.is_ready());
    ps[pos].set_value(nullptr);
  }

  auto res = f.get();
  EXPECT_EQ(res.size(), 5u);
  std::size_t idx = 0;
  for (auto& fc: res) {
    ASSERT_TRUE(fc.valid());
    ASSERT_TRUE(fc.is_ready());
    if (idx++ == 4)
      EXPECT_RUNTIME_ERROR(fc, "epic fail");
    else
      EXPECT_EQ(fc.get(), nullptr);
  }
}

TEST(WhenAllVectorTest, multiple_shared_futures_one_initionally_error) {
  experimental::promise<std::size_t> ps[5];
  experimental::shared_future<std::size_t> fs[5];

  std::transform(
    std::begin(ps), std::end(ps),
    std::begin(fs),
    std::mem_fn(&experimental::promise<std::size_t>::get_future)
  );
  ps[4].set_exception(std::make_exception_ptr(std::runtime_error("epic fail")));
  ASSERT_TRUE(fs[4].is_ready());

  auto f = experimental::when_all(std::begin(fs), std::end(fs));
  ASSERT_TRUE(f.valid());

  for (std::size_t pos: {0, 1, 3, 2}) {
    EXPECT_FALSE(f.is_ready());
    ps[pos].set_value(17*pos);
  }

  auto res = f.get();
  EXPECT_EQ(res.size(), 5u);
  std::size_t idx = 0;
  for (auto& fc: res) {
    ASSERT_TRUE(fc.valid());
    ASSERT_TRUE(fc.is_ready());
    if (idx == 4)
      EXPECT_RUNTIME_ERROR(fc, "epic fail");
    else
      EXPECT_EQ(fc.get(), 17*idx);
    ++idx;
  }
}

TEST(WhenAllVectorTest, futures_becomes_ready_concurrently) {
  experimental::promise<std::size_t> ps[3];
  experimental::shared_future<std::size_t> fs[3];

  std::transform(
    std::begin(ps), std::end(ps),
    std::begin(fs),
    std::mem_fn(&experimental::promise<size_t>::get_future)
  );
  auto f = experimental::when_all(std::begin(fs), std::end(fs));
  ASSERT_TRUE(f.valid());
  EXPECT_FALSE(f.is_ready());

  experimental::latch latch{4};
  std::size_t idx = 0;
  for (auto& p: ps) {
    g_future_tests_env->run_async([val = 5*idx++, &latch, p = std::move(p)]() mutable {
      latch.count_down_and_wait();
      p.set_value(val);
    });
  }

  latch.count_down_and_wait();
  auto res = f.get();
  idx = 0;
  for (auto& fc: res) {
    ASSERT_TRUE(fc.valid());
    ASSERT_TRUE(fc.is_ready());
    EXPECT_EQ(fc.get(), 5*idx++);
  }
}

} // anonymous namespace
