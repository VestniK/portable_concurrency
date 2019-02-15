#include <future>

#include <gtest/gtest.h>

#include <portable_concurrency/future>

#include "test_tools.h"

namespace portable_concurrency {
namespace {
namespace test {

struct SharedFutureThenUnwrap : future_test {
  pc::promise<int> promise;
  const pc::shared_future<int> future = promise.get_future();
};

TEST_F(SharedFutureThenUnwrap, future_unwrapped) {
  pc::promise<void> inner_promise;
  auto cnt_f = future.then(
      [inner_future = inner_promise.get_future()](pc::shared_future<int>) mutable { return std::move(inner_future); });
  static_assert(std::is_same<decltype(cnt_f), pc::future<void>>::value, "");
}

TEST_F(SharedFutureThenUnwrap, shared_future_unwrapped) {
  pc::promise<void> inner_promise;
  auto cnt_f = future.then([inner_future = inner_promise.get_future().share()](
                               pc::shared_future<int>) mutable { return std::move(inner_future); });
  static_assert(std::is_same<decltype(cnt_f), pc::shared_future<void>>::value, "");
}

TEST_F(SharedFutureThenUnwrap, result_is_not_ready_after_continuation_call) {
  pc::promise<void> inner_promise;
  pc::future<void> cnt_f = future.then([&](pc::shared_future<int>) { return inner_promise.get_future(); });
  promise.set_value(42);
  EXPECT_FALSE(cnt_f.is_ready());
}

TEST_F(SharedFutureThenUnwrap, result_is_ready_after_continuation_result_becomes_ready) {
  pc::promise<void> inner_promise;
  pc::future<void> cnt_f = future.then([&](pc::shared_future<int>) { return inner_promise.get_future(); });
  promise.set_value(42);
  inner_promise.set_value();
  EXPECT_TRUE(cnt_f.is_ready());
}

TEST_F(SharedFutureThenUnwrap, result_carries_broken_promise_for_invalid_result_of_continuation) {
  pc::future<std::string> cnt_f = future.then([](pc::shared_future<int>) { return pc::future<std::string>{}; });
  promise.set_value(42);
  EXPECT_FUTURE_ERROR(cnt_f.get(), std::future_errc::broken_promise);
}

TEST_F(SharedFutureThenUnwrap, result_propagates_inner_future_error) {
  pc::promise<void> inner_promise;
  pc::future<void> cnt_f = future.then([&](pc::shared_future<int>) { return inner_promise.get_future(); });
  promise.set_value(42);
  inner_promise.set_exception(std::make_exception_ptr(std::runtime_error{"Ooups"}));
  EXPECT_RUNTIME_ERROR(cnt_f, "Ooups");
}

TEST_F(SharedFutureThenUnwrap, exception_from_unwrapped_continuation_propagated_to_result) {
  pc::future<std::unique_ptr<int>> cnt_f = future.then(
      [](pc::shared_future<int>) -> pc::future<std::unique_ptr<int>> { throw std::runtime_error("Ooups"); });
  promise.set_value(42);
  EXPECT_RUNTIME_ERROR(cnt_f, "Ooups");
}

TEST_F(SharedFutureThenUnwrap, run_continuation_on_specific_executor) {
  pc::future<std::thread::id> cnt_f = future.then(
      g_future_tests_env, [](pc::shared_future<int>) { return pc::make_ready_future(std::this_thread::get_id()); });
  promise.set_value(42);
  EXPECT_TRUE(g_future_tests_env->uses_thread(cnt_f.get()));
}

TEST_F(SharedFutureThenUnwrap, shared_future_and_unwrapped_shared_future_access_same_storage) {
  pc::promise<std::string> inner_promise;
  pc::shared_future<std::string> inner_future = inner_promise.get_future();
  pc::shared_future<std::string> cnt_f = future.then([inner_future](pc::shared_future<int>) { return inner_future; });

  promise.set_value(42);
  inner_promise.set_value("qwe");
  EXPECT_EQ(&inner_future.get(), &cnt_f.get());
}

} // namespace test
} // anonymous namespace
} // namespace portable_concurrency
