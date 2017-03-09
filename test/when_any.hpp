#pragma once

#include <gtest/gtest.h>

#include "concurrency/future"

#include "test_tools.h"

namespace {

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

} // anonymous namespace
