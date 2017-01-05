#pragma once

#include <string>
#include <memory>

#include <gtest/gtest.h>

#include "concurrency/future"

#include "test_tools.h"

namespace {

template<typename T>
class PromiseTest: public ::testing::Test {};
TYPED_TEST_CASE_P(PromiseTest);

namespace tests {

template<typename T>
void get_future_twice() {
  concurrency::promise<T> p;
  concurrency::future<T> f1, f2;
  EXPECT_NO_THROW(f1 = p.get_future());
  EXPECT_FUTURE_ERROR(
    f2 = p.get_future(),
    std::future_errc::future_already_retrieved
  );
}

template<typename T>
void set_val_on_promise_without_future() {
  concurrency::promise<T> p;
  EXPECT_NO_THROW(p.set_value(some_value<T>()));
}

template<>
void set_val_on_promise_without_future<void>(){
  concurrency::promise<void> p;
  EXPECT_NO_THROW(p.set_value());
}

template<typename T>
void set_err_on_promise_without_future() {
  concurrency::promise<T> p;
  EXPECT_NO_THROW(p.set_exception(std::make_exception_ptr(std::runtime_error("error"))));
}

} // namespace tests

TYPED_TEST_P(PromiseTest, get_future_twice) {tests::get_future_twice<TypeParam>();}
TYPED_TEST_P(PromiseTest, set_val_on_promise_without_future) {tests::set_val_on_promise_without_future<TypeParam>();}
TYPED_TEST_P(PromiseTest, set_err_on_promise_without_future) {tests::set_err_on_promise_without_future<TypeParam>();}
REGISTER_TYPED_TEST_CASE_P(
  PromiseTest,
  get_future_twice,
  set_val_on_promise_without_future,
  set_err_on_promise_without_future
);

INSTANTIATE_TYPED_TEST_CASE_P(VoidType, PromiseTest, void);
INSTANTIATE_TYPED_TEST_CASE_P(PrimitiveType, PromiseTest, int);
INSTANTIATE_TYPED_TEST_CASE_P(CopyableType, PromiseTest, std::string);
INSTANTIATE_TYPED_TEST_CASE_P(MoveableType, PromiseTest, std::unique_ptr<int>);
INSTANTIATE_TYPED_TEST_CASE_P(ReferenceType, PromiseTest, future_tests_env&);

} // anonymous namespace
