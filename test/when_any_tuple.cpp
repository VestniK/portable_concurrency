#include <gtest/gtest.h>

#include "portable_concurrency/future"
#include "portable_concurrency/latch"

#include "test_tools.h"

using namespace std::literals;

namespace {

class WhenAnyTupleTest: public ::testing::TestWithParam<size_t> {};

TEST(WhenAnyTupleTest, empty_sequence) {
  auto res_fut = pc::when_any();
  ASSERT_TRUE(res_fut.valid());
  ASSERT_TRUE(res_fut.is_ready());

  auto res = res_fut.get();
  EXPECT_EQ(res.index, static_cast<std::size_t>(-1));
  EXPECT_EQ(std::tuple_size<decltype(res.futures)>::value, 0u);
}

TEST(WhenAnyTupleTest, single_future) {
  pc::promise<int> p;
  auto raw_f = p.get_future();
  auto f = pc::when_any(std::move(raw_f));
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

TEST(WhenAnyTupleTest, single_shared_future) {
  pc::promise<int> p;
  auto raw_f = p.get_future().share();
  auto f = pc::when_any(raw_f);
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

TEST(WhenAnyTupleTest, single_ready_future) {
  auto raw_f = pc::make_ready_future(123);
  auto f = pc::when_any(std::move(raw_f));
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

TEST(WhenAnyTupleTest, single_ready_shared_future) {
  auto raw_f = pc::make_ready_future(123).share();
  auto f = pc::when_any(raw_f);
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

TEST(WhenAnyTupleTest, single_error_future) {
  auto raw_f = pc::make_exceptional_future<int>(std::runtime_error("future with error"));
  auto f = pc::when_any(std::move(raw_f));
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

TEST(WhenAnyTupleTest, single_error_shared_future) {
  auto raw_f = pc::make_exceptional_future<int>(std::runtime_error("future with error")).share();
  auto f = pc::when_any(raw_f);
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

TEST(WhenAnyTupleTest, next_ready_futures_dont_affect_result_before_get) {
  pc::promise<int> p0;
  pc::promise<std::string> p1;
  pc::promise<void> p2;

  auto f = pc::when_any(p0.get_future(), p1.get_future(), p2.get_future());
  ASSERT_TRUE(f.valid());
  EXPECT_FALSE(f.is_ready());

  p0.set_value(12345);
  ASSERT_TRUE(f.valid());
  EXPECT_TRUE(f.is_ready());

  p1.set_value("qwe");
  ASSERT_TRUE(f.valid());
  EXPECT_TRUE(f.is_ready());

  p2.set_value();
  ASSERT_TRUE(f.valid());
  ASSERT_TRUE(f.is_ready());

  auto res = f.get();
  EXPECT_EQ(res.index, 0u);
}

TEST(WhenAnyTupleTest, next_ready_futures_dont_affect_result_after_get) {
  pc::promise<int> p0;
  pc::promise<std::string> p1;
  pc::promise<void> p2;

  auto f = pc::when_any(p0.get_future(), p1.get_future(), p2.get_future());
  ASSERT_TRUE(f.valid());
  EXPECT_FALSE(f.is_ready());

  p1.set_value("qwe");
  ASSERT_TRUE(f.valid());
  ASSERT_TRUE(f.is_ready());

  auto res = f.get();
  EXPECT_EQ(res.index, 1u);
  EXPECT_TRUE(std::get<1>(res.futures).is_ready());

  EXPECT_FALSE(std::get<0>(res.futures).is_ready());
  p0.set_value(12345);
  EXPECT_FALSE(f.valid());
  EXPECT_TRUE(std::get<0>(res.futures).is_ready());

  EXPECT_FALSE(std::get<2>(res.futures).is_ready());
  p2.set_value();
  EXPECT_FALSE(f.valid());
  EXPECT_TRUE(std::get<2>(res.futures).is_ready());
}

TEST(WhenAnyTupleTest, concurrent_result_delivery) {
  pc::latch latch{3};
  pc::packaged_task<int()> t0{[&latch] {latch.count_down_and_wait(); return 42;}};
  pc::promise<std::string> p1;
  pc::packaged_task<void()> t2{[&latch] {latch.count_down_and_wait();}};

  auto f = pc::when_any(t0.get_future(), p1.get_future(), t2.get_future());
  ASSERT_TRUE(f.valid());
  EXPECT_FALSE(f.is_ready());

  g_future_tests_env->run_async(std::move(t0));
  g_future_tests_env->run_async(std::move(t2));

  latch.count_down_and_wait();
  auto res = f.get();
  EXPECT_TRUE(res.index == 0u || res.index == 2u) << "unexpected index: " << res.index;
}

TEST_P(WhenAnyTupleTest, multiple_futures) {
  pc::promise<int> p0;
  auto raw_f0 = p0.get_future();
  pc::promise<std::string> p1;
  auto raw_f1 = p1.get_future().share();
  pc::promise<std::unique_ptr<int>> p2;
  auto raw_f2 = p2.get_future();
  pc::promise<void> p3;
  auto raw_f3 = p3.get_future().share();
  pc::promise<future_tests_env&> p4;
  auto raw_f4 = p4.get_future();

  auto f = pc::when_any(
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

TEST_P(WhenAnyTupleTest, multiple_futures_one_initially_ready) {
  pc::promise<int> p0;
  pc::promise<std::string> p1;
  pc::promise<std::unique_ptr<int>> p2;
  pc::promise<void> p3;
  pc::promise<future_tests_env&> p4;

  pc::shared_future<int> raw_f0 = p0.get_future();
  pc::future<std::string> raw_f1 = p1.get_future();
  pc::shared_future<std::unique_ptr<int>> raw_f2 = p2.get_future();
  pc::future<void> raw_f3 = p3.get_future();
  pc::shared_future<future_tests_env&> raw_f4 = p4.get_future();

  size_t idx_first = GetParam();
  switch (idx_first) {
  case 0: p0.set_value(345); break;
  case 1: p1.set_value("hello world"); break;
  case 2: p2.set_value(std::make_unique<int>(42)); break;
  case 3: p3.set_value(); break;
  case 4: p4.set_value(*g_future_tests_env); break;
  }

  auto f = pc::when_any(
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
INSTANTIATE_TEST_CASE_P(AllVals, WhenAnyTupleTest, ::testing::ValuesIn(first_idx_vals));

} // anonymous namespace
