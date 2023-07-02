#include <algorithm>
#include <list>

#include <gtest/gtest.h>

#include <portable_concurrency/future>
#include <portable_concurrency/latch>

#include "simple_arena_allocator.h"
#include "test_helpers.h"

namespace {

TEST(WhenAllVectorTest, empty_sequence) {
  std::list<pc::future<int>> empty;
  auto f = pc::when_all(empty.begin(), empty.end());

  ASSERT_TRUE(f.valid());
  ASSERT_TRUE(f.is_ready());

  std::vector<pc::future<int>> res = f.get();
  EXPECT_EQ(res.size(), 0u);
}

TEST(WhenAllVectorTest, single_future) {
  pc::promise<std::string> p;
  auto raw_f = p.get_future();
  auto f = pc::when_all(&raw_f, &raw_f + 1);
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
  pc::promise<std::unique_ptr<int>> p;
  auto raw_f = p.get_future().share();
  auto f = pc::when_all(&raw_f, &raw_f + 1);
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
  auto raw_f = pc::make_ready_future(123);
  auto f = pc::when_all(&raw_f, &raw_f + 1);
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
  auto raw_f = pc::make_ready_future(123).share();
  auto f = pc::when_all(&raw_f, &raw_f + 1);
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
  auto raw_f = pc::make_exceptional_future<future_tests_env &>(
      std::runtime_error("panic"));
  auto f = pc::when_all(&raw_f, &raw_f + 1);
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
  auto raw_f = pc::make_exceptional_future<future_tests_env &>(
                   std::runtime_error("panic"))
                   .share();
  auto f = pc::when_all(&raw_f, &raw_f + 1);
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
  pc::promise<int> ps[5];
  pc::future<int> fs[5];

  std::transform(std::begin(ps), std::end(ps), std::begin(fs),
                 get_promise_future);

  auto f = pc::when_all(std::begin(fs), std::end(fs));
  ASSERT_TRUE(f.valid());
  EXPECT_FALSE(f.is_ready());
  for (const auto &fi : fs)
    EXPECT_FALSE(fi.valid());

  for (std::size_t pos : {3, 0, 1, 4, 2}) {
    EXPECT_FALSE(f.is_ready());
    ps[pos].set_value(static_cast<int>(42 * pos));
  }
  ASSERT_TRUE(f.is_ready());

  auto res = f.get();
  EXPECT_EQ(res.size(), 5u);
  int idx = 0;
  for (auto &fc : res) {
    ASSERT_TRUE(fc.valid());
    ASSERT_TRUE(fc.is_ready());
    EXPECT_EQ(fc.get(), 42 * idx++);
  }
}

TEST(WhenAllVectorTest, multiple_shared_futures) {
  pc::promise<std::string> ps[5];
  pc::shared_future<std::string> fs[5];

  std::transform(std::begin(ps), std::end(ps), std::begin(fs),
                 get_promise_future);

  auto f = pc::when_all(std::begin(fs), std::end(fs));
  ASSERT_TRUE(f.valid());
  EXPECT_FALSE(f.is_ready());
  for (const auto &fi : fs)
    EXPECT_TRUE(fi.valid());

  for (std::size_t pos : {3, 0, 1, 4, 2}) {
    EXPECT_FALSE(f.is_ready());
    ps[pos].set_value(to_string(42 * pos));
  }
  ASSERT_TRUE(f.is_ready());

  auto res = f.get();
  EXPECT_EQ(res.size(), 5u);
  std::size_t idx = 0;
  for (auto &fc : res) {
    ASSERT_TRUE(fc.valid());
    ASSERT_TRUE(fc.is_ready());
    EXPECT_EQ(fc.get(), to_string(42 * idx++));
  }
}

TEST(WhenAllVectorTest, multiple_futures_one_initionally_ready) {
  pc::promise<void> ps[5];
  pc::future<void> fs[5];

  std::transform(std::begin(ps), std::end(ps), std::begin(fs),
                 get_promise_future);
  ps[1].set_value();
  ASSERT_TRUE(fs[1].is_ready());

  auto f = pc::when_all(std::begin(fs), std::end(fs));
  ASSERT_TRUE(f.valid());

  for (std::size_t pos : {4, 0, 3, 2}) {
    EXPECT_FALSE(f.is_ready());
    ps[pos].set_value();
  }
  ASSERT_TRUE(f.is_ready());

  auto res = f.get();
  EXPECT_EQ(res.size(), 5u);
  for (auto &fc : res) {
    ASSERT_TRUE(fc.valid());
    ASSERT_TRUE(fc.is_ready());
    ASSERT_NO_THROW(fc.get());
  }
}

TEST(WhenAllVectorTest, multiple_shared_futures_one_initionally_ready) {
  int some_vars[5] = {5, 4, 3, 2, 1};
  pc::promise<int &> ps[5];
  pc::shared_future<int &> fs[5];

  std::transform(std::begin(ps), std::end(ps), std::begin(fs),
                 get_promise_future);
  ps[0].set_value(some_vars[0]);
  ASSERT_TRUE(fs[0].is_ready());

  auto f = pc::when_all(std::begin(fs), std::end(fs));
  ASSERT_TRUE(f.valid());
  for (std::size_t pos : {4, 1, 3, 2}) {
    EXPECT_FALSE(f.is_ready());
    ps[pos].set_value(some_vars[pos]);
  }
  ASSERT_TRUE(f.is_ready());

  auto res = f.get();
  EXPECT_EQ(res.size(), 5u);
  std::size_t idx = 0;
  for (auto &fc : res) {
    ASSERT_TRUE(fc.valid());
    ASSERT_TRUE(fc.is_ready());
    EXPECT_EQ(&fc.get(), &some_vars[idx++]);
  }
}

TEST(WhenAllVectorTest, multiple_futures_one_initionally_error) {
  pc::promise<std::unique_ptr<int>> ps[5];
  pc::future<std::unique_ptr<int>> fs[5];

  std::transform(std::begin(ps), std::end(ps), std::begin(fs),
                 get_promise_future);
  ps[4].set_exception(std::make_exception_ptr(std::runtime_error("epic fail")));
  ASSERT_TRUE(fs[4].is_ready());

  auto f = pc::when_all(std::begin(fs), std::end(fs));
  ASSERT_TRUE(f.valid());

  for (std::size_t pos : {0, 1, 3, 2}) {
    EXPECT_FALSE(f.is_ready());
    ps[pos].set_value(nullptr);
  }

  auto res = f.get();
  EXPECT_EQ(res.size(), 5u);
  std::size_t idx = 0;
  for (auto &fc : res) {
    ASSERT_TRUE(fc.valid());
    ASSERT_TRUE(fc.is_ready());
    if (idx++ == 4)
      EXPECT_RUNTIME_ERROR(fc, "epic fail");
    else
      EXPECT_EQ(fc.get(), nullptr);
  }
}

TEST(WhenAllVectorTest, multiple_shared_futures_one_initionally_error) {
  pc::promise<std::size_t> ps[5];
  pc::shared_future<std::size_t> fs[5];

  std::transform(std::begin(ps), std::end(ps), std::begin(fs),
                 get_promise_future);
  ps[4].set_exception(std::make_exception_ptr(std::runtime_error("epic fail")));
  ASSERT_TRUE(fs[4].is_ready());

  auto f = pc::when_all(std::begin(fs), std::end(fs));
  ASSERT_TRUE(f.valid());

  for (std::size_t pos : {0, 1, 3, 2}) {
    EXPECT_FALSE(f.is_ready());
    ps[pos].set_value(17 * pos);
  }

  auto res = f.get();
  EXPECT_EQ(res.size(), 5u);
  std::size_t idx = 0;
  for (auto &fc : res) {
    ASSERT_TRUE(fc.valid());
    ASSERT_TRUE(fc.is_ready());
    if (idx == 4)
      EXPECT_RUNTIME_ERROR(fc, "epic fail");
    else
      EXPECT_EQ(fc.get(), 17 * idx);
    ++idx;
  }
}

TEST(WhenAllVectorTest, futures_becomes_ready_concurrently) {
  pc::promise<std::size_t> ps[3];
  pc::shared_future<std::size_t> fs[3];

  std::transform(std::begin(ps), std::end(ps), std::begin(fs),
                 get_promise_future);
  auto f = pc::when_all(std::begin(fs), std::end(fs));
  ASSERT_TRUE(f.valid());
  EXPECT_FALSE(f.is_ready());

  pc::latch latch{4};
  std::size_t idx = 0;
  for (auto &p : ps) {
    g_future_tests_env->run_async(
        [val = 5 * idx++, &latch, p = std::move(p)]() mutable {
          latch.count_down_and_wait();
          p.set_value(val);
        });
  }

  latch.count_down_and_wait();
  auto res = f.get();
  idx = 0;
  for (auto &fc : res) {
    ASSERT_TRUE(fc.valid());
    ASSERT_TRUE(fc.is_ready());
    EXPECT_EQ(fc.get(), 5 * idx++);
  }
}

TEST(when_all_on_vector, is_immediatelly_ready_on_empty_arg) {
  EXPECT_TRUE(pc::when_all(std::vector<pc::future<int>>{}).is_ready());
}

TEST(when_all_on_vector, is_immediatelly_ready_on_vector_of_ready_futures) {
  EXPECT_TRUE(pc::when_all(std::vector<pc::shared_future<int>>{
                               {pc::make_ready_future(42),
                                pc::make_ready_future(100500)}})
                  .is_ready());
}

TEST(when_all_on_vector, is_not_ready_on_vector_with_not_ready_futures) {
  pc::promise<int> p;
  std::vector<pc::future<int>> futures;
  futures.push_back(pc::make_ready_future(42));
  futures.push_back(p.get_future());
  EXPECT_FALSE(pc::when_all(std::move(futures)).is_ready());
}

TEST(when_all_on_vector, contains_vector_of_ready_futures_when_becomes_ready) {
  std::vector<pc::future<int>> futures;
  futures.push_back(pc::async(g_future_tests_env, [] { return 42; }));
  futures.push_back(pc::async(g_future_tests_env, [] { return 100500; }));
  EXPECT_TRUE(pc::when_all(std::move(futures))
                  .next([](std::vector<pc::future<int>> futures) {
                    return std::all_of(futures.begin(), futures.end(),
                                       pc::future_ready);
                  })
                  .get());
}

TEST(when_all_on_vector, reuses_buffer_of_passed_argument) {
  std::vector<pc::future<int>> futures;
  futures.reserve(2);
  const auto *buf_ptr = futures.data();
  futures.push_back(pc::async(g_future_tests_env, [] { return 42; }));
  futures.push_back(pc::async(g_future_tests_env, [] { return 100500; }));
  EXPECT_TRUE(pc::when_all(std::move(futures))
                  .next([buf_ptr](std::vector<pc::future<int>> futures) {
                    return futures.data() == buf_ptr;
                  })
                  .get());
}

TEST(when_all_on_vector, works_with_vector_using_custom_allocator) {
  static_arena<> arena;
  arena_vector<pc::future<int>> futures{{arena}};
  futures.reserve(2);
  const auto *buf_ptr = futures.data();
  futures.push_back(pc::async(g_future_tests_env, [] { return 42; }));
  futures.push_back(pc::async(g_future_tests_env, [] { return 100500; }));
  EXPECT_TRUE(pc::when_all(std::move(futures))
                  .next([buf_ptr](arena_vector<pc::future<int>> futures) {
                    return futures.data() == buf_ptr;
                  })
                  .get());
}

TEST(when_all_on_vector, preserves_order_of_futures) {
  static_arena<> arena;
  arena_vector<pc::future<int>> futures{{arena}};
  futures.reserve(2);
  futures.push_back(pc::async(g_future_tests_env, [] { return 42; }));
  futures.push_back(pc::async(g_future_tests_env, [] { return 100500; }));
  auto results = pc::when_all(std::move(futures))
                     .next([](arena_vector<pc::future<int>> futures) {
                       arena_vector<int> res{futures.get_allocator()};
                       std::transform(futures.begin(), futures.end(),
                                      std::back_inserter(res), pc::future_get);
                       return res;
                     })
                     .get();
  EXPECT_EQ(results, (arena_vector<int>{{42, 100500}, {arena}}));
}

} // anonymous namespace
