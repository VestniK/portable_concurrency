#pragma once

#include <gtest/gtest.h>

#include "concurrency/future"

#include "test_tools.h"

namespace {

class WhenAnyTest: public ::testing::TestWithParam<size_t> {};

TEST(WhenAnyTest, empty_sequence) {
  auto res_fut = experimental::when_any();
  ASSERT_TRUE(res_fut.valid());
  ASSERT_TRUE(res_fut.is_ready());

  auto res = res_fut.get();
  EXPECT_EQ(res.index, static_cast<std::size_t>(-1));
  EXPECT_EQ(std::tuple_size<decltype(res.futures)>::value, 0u);
}

TEST(WhenAnyTest, single_future) {
  experimental::promise<int> p;
  auto raw_f = p.get_future();
  auto f = experimental::when_any(std::move(raw_f));
  ASSERT_TRUE(f.valid());
  EXPECT_FALSE(raw_f.valid());
  EXPECT_FALSE(f.is_ready());

  p.set_value(42);
  ASSERT_TRUE(f.is_ready());

  auto res = f.get();
  EXPECT_EQ(res.index, 0u);
  EXPECT_EQ(std::tuple_size<decltype(res.futures)>::value, 1u);

  ASSERT_TRUE(std::get<0>(res.futures).valid());
  ASSERT_TRUE(std::get<0>(res.futures).is_ready());
  EXPECT_EQ(std::get<0>(res.futures).get(), 42);
}

TEST(WhenAnyTest, single_shared_future) {
  experimental::promise<int> p;
  auto raw_f = p.get_future().share();
  auto f = experimental::when_any(raw_f);
  ASSERT_TRUE(f.valid());
  EXPECT_TRUE(raw_f.valid());
  EXPECT_FALSE(f.is_ready());

  p.set_value(42);
  ASSERT_TRUE(f.is_ready());

  auto res = f.get();
  EXPECT_EQ(res.index, 0u);
  EXPECT_EQ(std::tuple_size<decltype(res.futures)>::value, 1u);

  ASSERT_TRUE(std::get<0>(res.futures).valid());
  ASSERT_TRUE(std::get<0>(res.futures).is_ready());
  EXPECT_EQ(std::get<0>(res.futures).get(), 42);
}

TEST(WhenAnyTest, single_ready_future) {
  auto raw_f = experimental::make_ready_future(123);
  auto f = experimental::when_any(std::move(raw_f));
  ASSERT_TRUE(f.valid());
  EXPECT_FALSE(raw_f.valid());
  EXPECT_TRUE(f.is_ready());

  auto res = f.get();
  EXPECT_EQ(res.index, 0u);
  EXPECT_EQ(std::tuple_size<decltype(res.futures)>::value, 1u);

  ASSERT_TRUE(std::get<0>(res.futures).valid());
  ASSERT_TRUE(std::get<0>(res.futures).is_ready());
  EXPECT_EQ(std::get<0>(res.futures).get(), 123);
}

TEST(WhenAnyTest, single_ready_shared_future) {
  auto raw_f = experimental::make_ready_future(123).share();
  auto f = experimental::when_any(raw_f);
  ASSERT_TRUE(f.valid());
  EXPECT_TRUE(raw_f.valid());
  EXPECT_TRUE(f.is_ready());

  auto res = f.get();
  EXPECT_EQ(res.index, 0u);
  EXPECT_EQ(std::tuple_size<decltype(res.futures)>::value, 1u);

  ASSERT_TRUE(std::get<0>(res.futures).valid());
  ASSERT_TRUE(std::get<0>(res.futures).is_ready());
  EXPECT_EQ(std::get<0>(res.futures).get(), 123);
}

TEST(WhenAnyTest, single_error_future) {
  auto raw_f = experimental::make_exceptional_future<int>(std::runtime_error("future with error"));
  auto f = experimental::when_any(std::move(raw_f));
  ASSERT_TRUE(f.valid());
  EXPECT_FALSE(raw_f.valid());
  EXPECT_TRUE(f.is_ready());

  auto res = f.get();
  EXPECT_EQ(res.index, 0u);
  EXPECT_EQ(std::tuple_size<decltype(res.futures)>::value, 1u);

  ASSERT_TRUE(std::get<0>(res.futures).valid());
  ASSERT_TRUE(std::get<0>(res.futures).is_ready());
  EXPECT_RUNTIME_ERROR(std::get<0>(res.futures), "future with error");
}

TEST(WhenAnyTest, single_error_shared_future) {
  auto raw_f = experimental::make_exceptional_future<int>(std::runtime_error("future with error")).share();
  auto f = experimental::when_any(raw_f);
  ASSERT_TRUE(f.valid());
  EXPECT_TRUE(raw_f.valid());
  EXPECT_TRUE(f.is_ready());

  auto res = f.get();
  EXPECT_EQ(res.index, 0u);
  EXPECT_EQ(std::tuple_size<decltype(res.futures)>::value, 1u);

  ASSERT_TRUE(std::get<0>(res.futures).valid());
  ASSERT_TRUE(std::get<0>(res.futures).is_ready());
  EXPECT_RUNTIME_ERROR(std::get<0>(res.futures), "future with error");
}

TEST_P(WhenAnyTest, multiple_futures) {
  experimental::promise<int> p0;
  auto raw_f0 = p0.get_future();
  experimental::promise<std::string> p1;
  auto raw_f1 = p1.get_future().share();
  experimental::promise<std::unique_ptr<int>> p2;
  auto raw_f2 = p2.get_future();
  experimental::promise<void> p3;
  auto raw_f3 = p3.get_future().share();
  experimental::promise<future_tests_env&> p4;
  auto raw_f4 = p4.get_future();

  auto f = experimental::when_any(
    std::move(raw_f0),
    raw_f1,
    std::move(raw_f2),
    raw_f3,
    std::move(raw_f4)
  );
  ASSERT_TRUE(f.valid());
  EXPECT_FALSE(raw_f0.valid());
  EXPECT_TRUE(raw_f1.valid());
  EXPECT_FALSE(raw_f2.valid());
  EXPECT_TRUE(raw_f3.valid());
  EXPECT_FALSE(raw_f4.valid());
  EXPECT_FALSE(f.is_ready());

  size_t idx_first = GetParam();
  switch (idx_first) {
  case 0: p0.set_value(345); break;
  case 1: p1.set_value("hello world"); break;
  case 2: p2.set_value(std::make_unique<int>(42)); break;
  case 3: p3.set_value(); break;
  case 4: p4.set_value(*g_future_tests_env); break;
  }
  ASSERT_TRUE(f.is_ready());

  auto res = f.get();
  EXPECT_EQ(res.index, idx_first);
  EXPECT_EQ(std::tuple_size<decltype(res.futures)>::value, 5u);

  ASSERT_TRUE(std::get<0>(res.futures).valid());
  ASSERT_TRUE(std::get<1>(res.futures).valid());
  ASSERT_TRUE(std::get<2>(res.futures).valid());
  ASSERT_TRUE(std::get<3>(res.futures).valid());
  ASSERT_TRUE(std::get<4>(res.futures).valid());

  ASSERT_TRUE(std::get<0>(res.futures).is_ready() || idx_first != 0);
  ASSERT_TRUE(std::get<1>(res.futures).is_ready() || idx_first != 1);
  ASSERT_TRUE(std::get<2>(res.futures).is_ready() || idx_first != 2);
  ASSERT_TRUE(std::get<3>(res.futures).is_ready() || idx_first != 3);
  ASSERT_TRUE(std::get<4>(res.futures).is_ready() || idx_first != 4);

  switch (idx_first) {
  case 0: EXPECT_EQ(std::get<0>(res.futures).get(), 345); break;
  case 1: EXPECT_EQ(std::get<1>(res.futures).get(), "hello world"s); break;
  case 2: EXPECT_EQ(*std::get<2>(res.futures).get(), 42); break;
  case 3: EXPECT_NO_THROW(std::get<3>(res.futures).get()); break;
  case 4: EXPECT_EQ(&std::get<4>(res.futures).get(), g_future_tests_env); break;
  }
}

TEST_P(WhenAnyTest, multiple_futures_one_initially_ready) {
  experimental::promise<int> p0;
  experimental::promise<std::string> p1;
  experimental::promise<std::unique_ptr<int>> p2;
  experimental::promise<void> p3;
  experimental::promise<future_tests_env&> p4;

  experimental::shared_future<int> raw_f0 = p0.get_future();
  experimental::future<std::string> raw_f1 = p1.get_future();
  experimental::shared_future<std::unique_ptr<int>> raw_f2 = p2.get_future();
  experimental::future<void> raw_f3 = p3.get_future();
  experimental::shared_future<future_tests_env&> raw_f4 = p4.get_future();

  size_t idx_first = GetParam();
  switch (idx_first) {
  case 0: p0.set_value(345); break;
  case 1: p1.set_value("hello world"); break;
  case 2: p2.set_value(std::make_unique<int>(42)); break;
  case 3: p3.set_value(); break;
  case 4: p4.set_value(*g_future_tests_env); break;
  }

  auto f = experimental::when_any(
    raw_f0,
    std::move(raw_f1),
    raw_f2,
    std::move(raw_f3),
    raw_f4
  );
  ASSERT_TRUE(f.valid());
  EXPECT_TRUE(raw_f0.valid());
  EXPECT_FALSE(raw_f1.valid());
  EXPECT_TRUE(raw_f2.valid());
  EXPECT_FALSE(raw_f3.valid());
  EXPECT_TRUE(raw_f4.valid());

  ASSERT_TRUE(f.is_ready());

  auto res = f.get();
  EXPECT_EQ(res.index, idx_first);
  EXPECT_EQ(std::tuple_size<decltype(res.futures)>::value, 5u);

  ASSERT_TRUE(std::get<0>(res.futures).valid());
  ASSERT_TRUE(std::get<1>(res.futures).valid());
  ASSERT_TRUE(std::get<2>(res.futures).valid());
  ASSERT_TRUE(std::get<3>(res.futures).valid());
  ASSERT_TRUE(std::get<4>(res.futures).valid());

  ASSERT_TRUE(std::get<0>(res.futures).is_ready() || idx_first != 0);
  ASSERT_TRUE(std::get<1>(res.futures).is_ready() || idx_first != 1);
  ASSERT_TRUE(std::get<2>(res.futures).is_ready() || idx_first != 2);
  ASSERT_TRUE(std::get<3>(res.futures).is_ready() || idx_first != 3);
  ASSERT_TRUE(std::get<4>(res.futures).is_ready() || idx_first != 4);

  switch (idx_first) {
  case 0: EXPECT_EQ(std::get<0>(res.futures).get(), 345); break;
  case 1: EXPECT_EQ(std::get<1>(res.futures).get(), "hello world"s); break;
  case 2: EXPECT_EQ(*std::get<2>(res.futures).get(), 42); break;
  case 3: EXPECT_NO_THROW(std::get<3>(res.futures).get()); break;
  case 4: EXPECT_EQ(&std::get<4>(res.futures).get(), g_future_tests_env); break;
  }
}

size_t first_idx_vals[] = {0, 1, 2, 3, 4};
INSTANTIATE_TEST_CASE_P(AllVals, WhenAnyTest, ::testing::ValuesIn(first_idx_vals));

} // anonymous namespace
