#include <future>

#include <gtest/gtest.h>

#include <portable_concurrency/future>

#include "test_tools.h"

namespace portable_concurrency {
namespace {
namespace test {

struct FutureThenUnwrap : future_test {
  FutureThenUnwrap() { std::tie(promise, future) = pc::make_promise<int>(); }
  pc::promise<int> promise;
  pc::future<int> future;
};

TEST_F(FutureThenUnwrap, future_unwrapped) {
  auto inner_promise = pc::make_promise<void>();
  auto cnt_f = future.then(
      [inner_future = std::move(inner_promise.second)](
          pc::future<int>) mutable { return std::move(inner_future); });
  static_assert(std::is_same<decltype(cnt_f), pc::future<void>>::value, "");
}

TEST_F(FutureThenUnwrap, shared_future_unwrapped) {
  auto inner_promise = pc::make_promise<void>();
  auto cnt_f = future.then(
      [inner_future = inner_promise.second.share()](pc::future<int>) mutable {
        return std::move(inner_future);
      });
  static_assert(std::is_same<decltype(cnt_f), pc::shared_future<void>>::value,
                "");
}

TEST_F(FutureThenUnwrap, result_is_not_ready_after_continuation_call) {
  auto inner_promise = pc::make_promise<void>();
  pc::future<void> cnt_f = future.then(
      [&](pc::future<int>) { return std::move(inner_promise.second); });
  promise.set_value(42);
  EXPECT_FALSE(cnt_f.is_ready());
}

TEST_F(FutureThenUnwrap,
       result_is_ready_after_continuation_result_becomes_ready) {
  auto inner_promise = pc::make_promise<void>();
  pc::future<void> cnt_f = future.then(
      [&](pc::future<int>) { return std::move(inner_promise.second); });
  promise.set_value(42);
  inner_promise.first.set_value();
  EXPECT_TRUE(cnt_f.is_ready());
}

TEST_F(FutureThenUnwrap, result_propagates_inner_future_error) {
  auto inner_promise = pc::make_promise<void>();
  pc::future<void> cnt_f = future.then(
      [&](pc::future<int>) { return std::move(inner_promise.second); });
  promise.set_value(42);
  inner_promise.first.set_exception(
      std::make_exception_ptr(std::runtime_error{"Ooups"}));
  EXPECT_RUNTIME_ERROR(cnt_f, "Ooups");
}

TEST_F(FutureThenUnwrap,
       exception_from_unwrapped_continuation_propagated_to_result) {
  pc::future<std::unique_ptr<int>> cnt_f =
      future.then([](pc::future<int>) -> pc::future<std::unique_ptr<int>> {
        throw std::runtime_error("Ooups");
      });
  promise.set_value(42);
  EXPECT_RUNTIME_ERROR(cnt_f, "Ooups");
}

TEST_F(FutureThenUnwrap, run_continuation_on_specific_executor) {
  pc::future<std::thread::id> cnt_f =
      future.then(g_future_tests_env, [](pc::future<int>) {
        return pc::make_ready_future(std::this_thread::get_id());
      });
  promise.set_value(42);
  EXPECT_TRUE(g_future_tests_env->uses_thread(cnt_f.get()));
}

TEST_F(FutureThenUnwrap,
       shared_future_and_unwrapped_shared_future_access_same_storage) {
  auto inner_promise = pc::make_promise<std::string>();
  pc::shared_future<std::string> inner_future = std::move(inner_promise.second);
  pc::shared_future<std::string> cnt_f =
      future.then([inner_future](pc::future<int>) { return inner_future; });

  promise.set_value(42);
  inner_promise.first.set_value("qwe");
  EXPECT_EQ(&inner_future.get(), &cnt_f.get());
}

} // namespace test
} // anonymous namespace
} // namespace portable_concurrency
