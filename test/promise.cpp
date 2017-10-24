#include <string>
#include <memory>

#include <gtest/gtest.h>

#include "portable_concurrency/future"

#include "test_helpers.h"
#include "test_tools.h"

namespace {

template<typename T>
struct PromiseTest: ::testing::Test {};
TYPED_TEST_CASE(PromiseTest, TestTypes);

namespace tests {

template<typename T>
void get_future_twice() {
  pc::promise<T> p;
  pc::future<T> f1, f2;
  EXPECT_NO_THROW(f1 = p.get_future());
  EXPECT_FUTURE_ERROR(
    f2 = p.get_future(),
    std::future_errc::future_already_retrieved
  );
}

template<typename T>
void set_val_on_promise_without_future() {
  pc::promise<T> p;
  EXPECT_NO_THROW(p.set_value(some_value<T>()));
}

template<>
void set_val_on_promise_without_future<void>(){
  pc::promise<void> p;
  EXPECT_NO_THROW(p.set_value());
}

template<typename T>
void set_err_on_promise_without_future() {
  pc::promise<T> p;
  EXPECT_NO_THROW(p.set_exception(std::make_exception_ptr(std::runtime_error("error"))));
}

template<typename T>
void set_value_twice_without_future() {
  pc::promise<T> p;
  EXPECT_NO_THROW(p.set_value(some_value<T>()));
  EXPECT_FUTURE_ERROR(p.set_value(some_value<T>()), std::future_errc::promise_already_satisfied);
}

template<>
void set_value_twice_without_future<void>() {
  pc::promise<void> p;
  EXPECT_NO_THROW(p.set_value());
  EXPECT_FUTURE_ERROR(p.set_value(), std::future_errc::promise_already_satisfied);
}

template<typename T>
void set_value_twice_with_future() {
  pc::promise<T> p;
  auto f = p.get_future();

  EXPECT_NO_THROW(p.set_value(some_value<T>()));
  EXPECT_TRUE(f.valid());
  EXPECT_TRUE(f.is_ready());

  EXPECT_FUTURE_ERROR(p.set_value(some_value<T>()), std::future_errc::promise_already_satisfied);
  EXPECT_TRUE(f.valid());
  EXPECT_TRUE(f.is_ready());
}

template<>
void set_value_twice_with_future<void>() {
  pc::promise<void> p;
  auto f = p.get_future();

  EXPECT_NO_THROW(p.set_value());
  EXPECT_TRUE(f.valid());
  EXPECT_TRUE(f.is_ready());

  EXPECT_FUTURE_ERROR(p.set_value(), std::future_errc::promise_already_satisfied);
  EXPECT_TRUE(f.valid());
  EXPECT_TRUE(f.is_ready());
}

template<typename T>
void set_value_twice_after_value_taken() {
  pc::promise<T> p;
  auto f = p.get_future();

  EXPECT_NO_THROW(p.set_value(some_value<T>()));
  EXPECT_TRUE(f.valid());
  EXPECT_TRUE(f.is_ready());
  f.get();
  EXPECT_FALSE(f.valid());

  EXPECT_FUTURE_ERROR(p.set_value(some_value<T>()), std::future_errc::promise_already_satisfied);
  EXPECT_FALSE(f.valid());
}

template<>
void set_value_twice_after_value_taken<void>() {
  pc::promise<void> p;
  auto f = p.get_future();

  EXPECT_NO_THROW(p.set_value());
  EXPECT_TRUE(f.valid());
  EXPECT_TRUE(f.is_ready());
  f.get();
  EXPECT_FALSE(f.valid());

  EXPECT_FUTURE_ERROR(p.set_value(), std::future_errc::promise_already_satisfied);
  EXPECT_FALSE(f.valid());
}

} // namespace tests

TYPED_TEST(PromiseTest, get_future_twice) {tests::get_future_twice<TypeParam>();}
TYPED_TEST(PromiseTest, set_val_on_promise_without_future) {tests::set_val_on_promise_without_future<TypeParam>();}
TYPED_TEST(PromiseTest, set_err_on_promise_without_future) {tests::set_err_on_promise_without_future<TypeParam>();}
TYPED_TEST(PromiseTest, set_value_twice_without_future) {tests::set_value_twice_without_future<TypeParam>();}
TYPED_TEST(PromiseTest, set_value_twice_with_future) {tests::set_value_twice_with_future<TypeParam>();}
TYPED_TEST(PromiseTest, set_value_twice_after_value_taken) {tests::set_value_twice_after_value_taken<TypeParam>();}

TYPED_TEST(PromiseTest, can_retreive_value_set_before_get_future) {
  pc::promise<TypeParam> promise;
  set_promise_value(promise);
  pc::future<TypeParam> future = promise.get_future();
  EXPECT_SOME_VALUE(future);
}

TYPED_TEST(PromiseTest, abandon_promise_set_proper_error) {
  pc::future<TypeParam> future;
  {
    pc::promise<TypeParam> promise;
    future = promise.get_future();
  }
  EXPECT_FUTURE_ERROR(future.get(), std::future_errc::broken_promise);
}

} // anonymous namespace
