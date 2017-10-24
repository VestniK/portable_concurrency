#include <gtest/gtest.h>

#include <portable_concurrency/future>

namespace portable_concurrency {
namespace test {
namespace {

TEST(PromiseCancelation, no_object_held_by_promise_after_future_destruction) {
  auto val = std::make_shared<int>(42);
  std::weak_ptr<int> weak = val;
  pc::promise<std::shared_ptr<int>> promise;
  promise.set_value(std::move(val));
  {
    auto future = promise.get_future();
  }
  EXPECT_FALSE(weak.lock());
}

TEST(PromiseCanceleation, set_value_do_nothing_on_canceled_promise) {
  pc::promise<int> promise;
  promise.get_future();
  EXPECT_NO_THROW(promise.set_value(42));
}

TEST(PromiseCanceleation, set_exception_do_nothing_on_canceled_promise) {
  pc::promise<int> promise;
  promise.get_future();
  EXPECT_NO_THROW(promise.set_exception(std::make_exception_ptr(std::runtime_error{"Ooups"})));
}

} // namespace
} // namespace test
} // namespace portable_concurrency
