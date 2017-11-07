#include <gtest/gtest.h>

#include <portable_concurrency/future>
#include <portable_concurrency/latch>

#include "test_tools.h"

using namespace std::literals;

namespace {

class WhenAllTupleTest: public ::testing::Test {};

TEST(WhenAllTupleTest, empty_sequence) {
  auto res_fut = pc::when_all();
  ASSERT_TRUE(res_fut.valid());
  ASSERT_TRUE(res_fut.is_ready());

  auto res = res_fut.get();
  EXPECT_EQ(std::tuple_size<decltype(res)>::value, 0u);
}

TEST(WhenAllTupleTest, single_future) {
  pc::promise<int> p;
  auto raw_f = p.get_future();
  auto f = pc::when_all(std::move(raw_f));
  ASSERT_TRUE(f.valid());
  EXPECT_FALSE(raw_f.valid());
  EXPECT_FALSE(f.is_ready());

  p.set_value(42);
  ASSERT_TRUE(f.is_ready());

  auto res = f.get();
  EXPECT_EQ(std::tuple_size<decltype(res)>::value, 1u);

  ASSERT_TRUE(std::get<0>(res).valid());
  ASSERT_TRUE(std::get<0>(res).is_ready());
  EXPECT_EQ(std::get<0>(res).get(), 42);
}

TEST(WhenAllTupleTest, single_shared_future) {
  pc::promise<int> p;
  auto raw_f = p.get_future().share();
  auto f = pc::when_all(raw_f);
  ASSERT_TRUE(f.valid());
  EXPECT_TRUE(raw_f.valid());
  EXPECT_FALSE(f.is_ready());

  p.set_value(42);
  ASSERT_TRUE(f.is_ready());

  auto res = f.get();
  EXPECT_EQ(std::tuple_size<decltype(res)>::value, 1u);

  ASSERT_TRUE(std::get<0>(res).valid());
  ASSERT_TRUE(std::get<0>(res).is_ready());
  EXPECT_EQ(std::get<0>(res).get(), 42);
}

TEST(WhenAllTupleTest, single_ready_future) {
  auto raw_f = pc::make_ready_future(123);
  auto f = pc::when_all(std::move(raw_f));
  ASSERT_TRUE(f.valid());
  EXPECT_FALSE(raw_f.valid());
  EXPECT_TRUE(f.is_ready());

  auto res = f.get();
  EXPECT_EQ(std::tuple_size<decltype(res)>::value, 1u);

  ASSERT_TRUE(std::get<0>(res).valid());
  ASSERT_TRUE(std::get<0>(res).is_ready());
  EXPECT_EQ(std::get<0>(res).get(), 123);
}

TEST(WhenAllTupleTest, single_ready_shared_future) {
  auto raw_f = pc::make_ready_future(123).share();
  auto f = pc::when_all(raw_f);
  ASSERT_TRUE(f.valid());
  EXPECT_TRUE(raw_f.valid());
  EXPECT_TRUE(f.is_ready());

  auto res = f.get();
  EXPECT_EQ(std::tuple_size<decltype(res)>::value, 1u);

  ASSERT_TRUE(std::get<0>(res).valid());
  ASSERT_TRUE(std::get<0>(res).is_ready());
  EXPECT_EQ(std::get<0>(res).get(), 123);
}

TEST(WhenAllTupleTest, single_error_future) {
  auto raw_f = pc::make_exceptional_future<int>(std::runtime_error("future with error"));
  auto f = pc::when_all(std::move(raw_f));
  ASSERT_TRUE(f.valid());
  EXPECT_FALSE(raw_f.valid());
  EXPECT_TRUE(f.is_ready());

  auto res = f.get();
  EXPECT_EQ(std::tuple_size<decltype(res)>::value, 1u);

  ASSERT_TRUE(std::get<0>(res).valid());
  ASSERT_TRUE(std::get<0>(res).is_ready());
  EXPECT_RUNTIME_ERROR(std::get<0>(res), "future with error");
}

TEST(WhenAllTupleTest, single_error_shared_future) {
  auto raw_f = pc::make_exceptional_future<int>(std::runtime_error("future with error")).share();
  auto f = pc::when_all(raw_f);
  ASSERT_TRUE(f.valid());
  EXPECT_TRUE(raw_f.valid());
  EXPECT_TRUE(f.is_ready());

  auto res = f.get();
  EXPECT_EQ(std::tuple_size<decltype(res)>::value, 1u);

  ASSERT_TRUE(std::get<0>(res).valid());
  ASSERT_TRUE(std::get<0>(res).is_ready());
  EXPECT_RUNTIME_ERROR(std::get<0>(res), "future with error");
}

TEST(WhenAllTupleTest, ready_on_all_only) {
  pc::promise<int> p0;
  pc::promise<std::string> p1;
  pc::promise<void> p2;

  auto f = pc::when_all(p0.get_future(), p1.get_future(), p2.get_future().share());
  ASSERT_TRUE(f.valid());
  EXPECT_FALSE(f.is_ready());

  p1.set_value("qwe");
  ASSERT_TRUE(f.valid());
  EXPECT_FALSE(f.is_ready());

  p0.set_value(42);
  ASSERT_TRUE(f.valid());
  EXPECT_FALSE(f.is_ready());

  p2.set_value();
  ASSERT_TRUE(f.valid());
  ASSERT_TRUE(f.is_ready());

  auto res = f.get();
  ASSERT_TRUE(std::get<0>(res).is_ready());
  EXPECT_EQ(std::get<0>(res).get(), 42);

  ASSERT_TRUE(std::get<1>(res).is_ready());
  EXPECT_EQ(std::get<1>(res).get(), "qwe");

  ASSERT_TRUE(std::get<2>(res).is_ready());
  ASSERT_NO_THROW(std::get<2>(res).get());
}

TEST(WhenAllTupleTest, concurrent_result_delivery) {
  pc::latch latch{4};
  pc::packaged_task<int()> t0([&latch] {latch.count_down_and_wait(); return 42;});
  pc::packaged_task<std::string()> t1([&latch] {latch.count_down_and_wait(); return "qwe"s;});
  pc::packaged_task<void()> t2([&latch] {latch.count_down_and_wait();});

  auto f = pc::when_all(t0.get_future().share(), t1.get_future(), t2.get_future());
  ASSERT_TRUE(f.valid());
  EXPECT_FALSE(f.is_ready());

  g_future_tests_env->run_async(std::move(t0));
  g_future_tests_env->run_async(std::move(t1));
  g_future_tests_env->run_async(std::move(t2));

  latch.count_down_and_wait();
  auto res = f.get();
  ASSERT_TRUE(std::get<0>(res).is_ready());
  EXPECT_EQ(std::get<0>(res).get(), 42);

  ASSERT_TRUE(std::get<1>(res).is_ready());
  EXPECT_EQ(std::get<1>(res).get(), "qwe");

  ASSERT_TRUE(std::get<2>(res).is_ready());
  ASSERT_NO_THROW(std::get<2>(res).get());
}

TEST(WhenAllTupleTest, some_initially_redy) {
  pc::promise<int> p0;
  auto f1 = pc::make_ready_future("qwe"s).share();
  pc::promise<void> p2;

  auto f = pc::when_all(p0.get_future(), f1, p2.get_future());
  ASSERT_TRUE(f.valid());
  ASSERT_TRUE(f1.valid());
  EXPECT_FALSE(f.is_ready());

  p0.set_value(42);
  ASSERT_TRUE(f.valid());
  EXPECT_FALSE(f.is_ready());

  p2.set_value();
  ASSERT_TRUE(f.valid());
  ASSERT_TRUE(f.is_ready());

  auto res = f.get();
  ASSERT_TRUE(std::get<0>(res).is_ready());
  EXPECT_EQ(std::get<0>(res).get(), 42);

  ASSERT_TRUE(std::get<1>(res).is_ready());
  EXPECT_EQ(std::get<1>(res).get(), "qwe");

  ASSERT_TRUE(std::get<2>(res).is_ready());
  ASSERT_NO_THROW(std::get<2>(res).get());
}

TEST(WhenAllTupleTest, all_initially_redy) {
  auto f0 = pc::make_ready_future(42).share();
  auto f1 = pc::make_ready_future("qwe"s);
  auto f2 = pc::make_ready_future().share();

  auto f = pc::when_all(f0, std::move(f1), f2);
  ASSERT_TRUE(f.valid());
  EXPECT_TRUE(f0.valid());
  EXPECT_FALSE(f1.valid());
  EXPECT_TRUE(f2.valid());
  ASSERT_TRUE(f.is_ready());

  auto res = f.get();
  ASSERT_TRUE(std::get<0>(res).is_ready());
  EXPECT_EQ(std::get<0>(res).get(), 42);

  ASSERT_TRUE(std::get<1>(res).is_ready());
  EXPECT_EQ(std::get<1>(res).get(), "qwe");

  ASSERT_TRUE(std::get<2>(res).is_ready());
  ASSERT_NO_THROW(std::get<2>(res).get());
}

} // anonymous namespace
