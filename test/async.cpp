#include <future>

#include <gtest/gtest.h>

#include <portable_concurrency/future>

#include "test_tools.h"

using namespace std::literals;

namespace portable_concurrency {
namespace {
namespace test {

struct Async : future_test {};

TEST_F(Async, returns_valid_future) {
  pc::future<int> future = pc::async(g_future_tests_env, [] { return 42; });
  EXPECT_TRUE(future.valid());
}

TEST_F(Async, delivers_function_result) {
  pc::future<int> future = pc::async(g_future_tests_env, [] { return 42; });
  EXPECT_EQ(future.get(), 42);
}

TEST_F(Async, executes_functor_on_specified_executor) {
  pc::future<std::thread::id> future = pc::async(g_future_tests_env, [] { return std::this_thread::get_id(); });
  EXPECT_TRUE(g_future_tests_env->uses_thread(future.get()));
}

TEST_F(Async, support_abandon_operation) {
  pc::future<void> future = pc::async(null_executor, [] {});
  EXPECT_FUTURE_ERROR(future.get(), std::future_errc::broken_promise);
}

TEST_F(Async, unwraps_future) {
  auto future = pc::async(g_future_tests_env, [] { return pc::async(g_future_tests_env, [] { return 100500; }); });
  static_assert(std::is_same<decltype(future), pc::future<int>>::value, "");
  EXPECT_EQ(future.get(), 100500);
}

TEST_F(Async, unwraps_shared_future) {
  auto future =
      pc::async(g_future_tests_env, [] { return pc::async(g_future_tests_env, [] { return 100500; }).share(); });
  static_assert(std::is_same<decltype(future), pc::shared_future<int>>::value, "");
  EXPECT_EQ(future.get(), 100500);
}

TEST_F(Async, captures_parameters) {
  pc::future<size_t> future = pc::async(g_future_tests_env, std::hash<std::string>{}, "qwe");
  EXPECT_EQ(future.get(), std::hash<std::string>{}("qwe"));
}

} // namespace test
} // anonymous namespace
} // namespace portable_concurrency
