#include <algorithm>
#include <list>

#include <gtest/gtest.h>

#include <portable_concurrency/future>
#include <portable_concurrency/latch>

#include "simple_arena_allocator.h"
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
  auto raw_f = pc::make_exceptional_future<future_tests_env &>(
      std::runtime_error("panic"));
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
  auto raw_f = pc::make_exceptional_future<future_tests_env &>(
                   std::runtime_error("panic"))
                   .share();
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

  std::transform(std::begin(ps), std::end(ps), std::begin(fs),
                 get_promise_future);

  auto f = pc::when_any(std::begin(fs), std::end(fs));
  ASSERT_TRUE(f.valid());
  EXPECT_FALSE(f.is_ready());
  for (const auto &fi : fs)
    EXPECT_FALSE(fi.valid());

  ps[3].set_value(42);
  ASSERT_TRUE(f.is_ready());

  auto res = f.get();
  EXPECT_EQ(res.index, 3u);
  EXPECT_EQ(res.futures.size(), 5u);
  std::size_t idx = 0;
  for (auto &fc : res.futures) {
    ASSERT_TRUE(fc.valid());
    EXPECT_EQ(fc.is_ready(), idx++ == res.index);
  }
  EXPECT_EQ(res.futures[res.index].get(), 42);
}

TEST(WhenAnyVectorTest, multiple_shared_futures) {
  pc::promise<std::string> ps[5];
  pc::shared_future<std::string> fs[5];

  std::transform(std::begin(ps), std::end(ps), std::begin(fs),
                 get_promise_future);

  auto f = pc::when_any(std::begin(fs), std::end(fs));
  ASSERT_TRUE(f.valid());
  EXPECT_FALSE(f.is_ready());
  for (const auto &fi : fs)
    EXPECT_TRUE(fi.valid());

  ps[2].set_value("42");
  ASSERT_TRUE(f.is_ready());

  auto res = f.get();
  EXPECT_EQ(res.index, 2u);
  EXPECT_EQ(res.futures.size(), 5u);
  std::size_t idx = 0;
  for (auto &fc : res.futures) {
    ASSERT_TRUE(fc.valid());
    EXPECT_EQ(fc.is_ready(), idx++ == res.index);
  }
  EXPECT_EQ(res.futures[res.index].get(), "42");
}

TEST(WhenAnyVectorTest, multiple_futures_one_initionally_ready) {
  pc::promise<void> ps[5];
  pc::future<void> fs[5];

  std::transform(std::begin(ps), std::end(ps), std::begin(fs),
                 get_promise_future);
  ps[1].set_value();
  ASSERT_TRUE(fs[1].is_ready());

  auto f = pc::when_any(std::begin(fs), std::end(fs));
  ASSERT_TRUE(f.valid());
  ASSERT_TRUE(f.is_ready());

  auto res = f.get();
  EXPECT_EQ(res.index, 1u);
  EXPECT_EQ(res.futures.size(), 5u);
  std::size_t idx = 0;
  for (auto &fc : res.futures) {
    ASSERT_TRUE(fc.valid());
    EXPECT_EQ(fc.is_ready(), idx++ == res.index);
  }
  ASSERT_NO_THROW(res.futures[res.index].get());
}

TEST(WhenAnyVectorTest, multiple_shared_futures_one_initionally_ready) {
  int some_var = 42;

  pc::promise<int &> ps[5];
  pc::shared_future<int &> fs[5];

  std::transform(std::begin(ps), std::end(ps), std::begin(fs),
                 get_promise_future);
  ps[0].set_value(some_var);
  ASSERT_TRUE(fs[0].is_ready());

  auto f = pc::when_any(std::begin(fs), std::end(fs));
  ASSERT_TRUE(f.valid());
  ASSERT_TRUE(f.is_ready());

  auto res = f.get();
  EXPECT_EQ(res.index, 0u);
  EXPECT_EQ(res.futures.size(), 5u);
  std::size_t idx = 0;
  for (auto &fc : res.futures) {
    ASSERT_TRUE(fc.valid());
    EXPECT_EQ(fc.is_ready(), idx++ == res.index);
  }
  EXPECT_EQ(&res.futures[res.index].get(), &some_var);
}

TEST(WhenAnyVectorTest, multiple_futures_one_initionally_error) {
  pc::promise<std::unique_ptr<int>> ps[5];
  pc::future<std::unique_ptr<int>> fs[5];

  std::transform(std::begin(ps), std::end(ps), std::begin(fs),
                 get_promise_future);
  ps[4].set_exception(std::make_exception_ptr(std::runtime_error("epic fail")));
  ASSERT_TRUE(fs[4].is_ready());

  auto f = pc::when_any(std::begin(fs), std::end(fs));
  ASSERT_TRUE(f.valid());
  ASSERT_TRUE(f.is_ready());

  auto res = f.get();
  EXPECT_EQ(res.index, 4u);
  EXPECT_EQ(res.futures.size(), 5u);
  std::size_t idx = 0;
  for (auto &fc : res.futures) {
    ASSERT_TRUE(fc.valid());
    EXPECT_EQ(fc.is_ready(), idx++ == res.index);
  }
  EXPECT_RUNTIME_ERROR(res.futures[res.index], "epic fail");
}

TEST(WhenAnyVectorTest, multiple_shared_futures_one_initionally_error) {
  pc::promise<int> ps[5];
  pc::shared_future<int> fs[5];

  std::transform(std::begin(ps), std::end(ps), std::begin(fs),
                 get_promise_future);
  ps[4].set_exception(std::make_exception_ptr(std::runtime_error("epic fail")));
  ASSERT_TRUE(fs[4].is_ready());

  auto f = pc::when_any(std::begin(fs), std::end(fs));
  ASSERT_TRUE(f.valid());
  ASSERT_TRUE(f.is_ready());

  auto res = f.get();
  EXPECT_EQ(res.index, 4u);
  EXPECT_EQ(res.futures.size(), 5u);
  std::size_t idx = 0;
  for (auto &fc : res.futures) {
    ASSERT_TRUE(fc.valid());
    EXPECT_EQ(fc.is_ready(), idx++ == res.index);
  }
  EXPECT_RUNTIME_ERROR(res.futures[res.index], "epic fail");
}

TEST(WhenAnyVectorTest, next_ready_futures_dont_affect_result_before_get) {
  pc::promise<size_t> ps[5];
  pc::shared_future<size_t> fs[5];

  std::transform(std::begin(ps), std::end(ps), std::begin(fs),
                 get_promise_future);

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

  std::transform(std::begin(ps), std::end(ps), std::begin(fs),
                 get_promise_future);

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

  std::transform(std::begin(ps), std::end(ps), std::begin(fs),
                 get_promise_future);
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
  EXPECT_TRUE(res.index == 1 || res.index == 2)
      << "unexpected index: " << res.index;
}

TEST(when_any_on_vector, is_immediatelly_ready_on_empty_arg) {
  EXPECT_TRUE(pc::when_any(std::vector<pc::future<int>>{}).is_ready());
}

TEST(when_any_on_vector, is_immediatelly_ready_on_vector_of_ready_futures) {
  EXPECT_TRUE(pc::when_any(std::vector<pc::shared_future<int>>{
                               {pc::make_ready_future(42),
                                pc::make_ready_future(100500)}})
                  .is_ready());
}

TEST(when_any_on_vector, is_ready_on_vector_with_ready_futures) {
  pc::promise<int> p;
  std::vector<pc::future<int>> futures;
  futures.push_back(pc::make_ready_future(42));
  futures.push_back(p.get_future());
  EXPECT_TRUE(pc::when_any(std::move(futures)).is_ready());
}

TEST(when_any_on_vector, is_not_ready_on_vector_of_not_ready_futures) {
  pc::promise<int> p[2];
  std::vector<pc::future<int>> futures;
  futures.push_back(p[0].get_future());
  futures.push_back(p[1].get_future());
  EXPECT_FALSE(pc::when_any(std::move(futures)).is_ready());
}

TEST(when_any_on_vector,
     contains_ready_future_at_returned_index_when_becomes_ready) {
  std::vector<pc::future<int>> futures;
  futures.push_back(pc::async(g_future_tests_env, [] { return 42; }));
  futures.push_back(pc::async(g_future_tests_env, [] { return 100500; }));
  EXPECT_TRUE(
      pc::when_any(std::move(futures))
          .next([](pc::when_any_result<std::vector<pc::future<int>>> res) {
            return res.futures[res.index].is_ready();
          })
          .get());
}

TEST(when_any_on_vector, reuses_buffer_of_passed_argument) {
  std::vector<pc::future<int>> futures;
  futures.reserve(2);
  const auto *buf_ptr = futures.data();
  futures.push_back(pc::async(g_future_tests_env, [] { return 42; }));
  futures.push_back(pc::async(g_future_tests_env, [] { return 100500; }));
  EXPECT_TRUE(
      pc::when_any(std::move(futures))
          .next(
              [buf_ptr](pc::when_any_result<std::vector<pc::future<int>>> res) {
                return res.futures.data() == buf_ptr;
              })
          .get());
}

TEST(when_any_on_vector, works_with_vector_using_custom_allocator) {
  static_arena<> arena;
  arena_vector<pc::future<int>> futures{{arena}};
  futures.reserve(2);
  const auto *buf_ptr = futures.data();
  futures.push_back(pc::async(g_future_tests_env, [] { return 42; }));
  futures.push_back(pc::async(g_future_tests_env, [] { return 100500; }));
  EXPECT_TRUE(
      pc::when_any(std::move(futures))
          .next([buf_ptr](
                    pc::when_any_result<arena_vector<pc::future<int>>> res) {
            return res.futures.data() == buf_ptr;
          })
          .get());
}

TEST(when_any_on_vector, preserves_order_of_futures) {
  static_arena<> arena;
  arena_vector<pc::future<int>> futures{{arena}};
  futures.reserve(2);
  futures.push_back(pc::async(g_future_tests_env, [] { return 42; }));
  futures.push_back(pc::async(g_future_tests_env, [] { return 100500; }));
  auto results =
      pc::when_any(std::move(futures))
          .next([](pc::when_any_result<arena_vector<pc::future<int>>> res) {
            std::vector<int> vals;
            std::transform(res.futures.begin(), res.futures.end(),
                           std::back_inserter(vals), pc::future_get);
            return vals;
          })
          .get();
  EXPECT_EQ(results, (std::vector<int>{{42, 100500}}));
}

} // anonymous namespace
