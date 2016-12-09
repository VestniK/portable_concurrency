#include <chrono>
#include <iostream>
#include <memory>
#include <string>
#include <thread>

#include <gtest/gtest.h>

#include "concurrency/future"

#include "test_tools.h"

using namespace std::literals;
using sys_clock = std::chrono::system_clock;
using hires_clock = std::chrono::high_resolution_clock;

namespace {

template<typename T, typename R, typename P>
auto set_value_in_other_thread(
  T&& val,
  std::chrono::duration<R, P> sleep_duration
) {
  using TV = std::decay_t<T>;
  concurrency::promise<TV> promise;
  auto res = promise.get_future();

  std::thread worker([](
    concurrency::promise<T>&& promise,
    T&& worker_val,
    std::chrono::duration<R, P> tm
  ) {
    std::this_thread::sleep_for(tm);
    promise.set_value(std::forward<T>(worker_val));
  }, std::move(promise), std::forward<T>(val), sleep_duration);
  worker.detach();

  return res;
}

template<typename T, typename E, typename R, typename P>
auto set_error_in_other_thread(
  E err,
  std::chrono::duration<R, P> sleep_duration
) {
  concurrency::promise<T> promise;
  auto res = promise.get_future();

  std::thread worker([](
    concurrency::promise<T>&& promise,
    E worker_err,
    std::chrono::duration<R, P> tm
  ) {
    std::this_thread::sleep_for(tm);
    promise.set_exception(std::make_exception_ptr(worker_err));
  }, std::move(promise), std::move(err), sleep_duration);
  worker.detach();

  return res;
}

template<typename T>
T val(int n);

template<>
int val<int> (int n) {return n;}

template<>
std::string val<std::string> (int n) {return std::to_string(n);}

template<>
std::unique_ptr<int> val<std::unique_ptr<int>> (int n) {return std::make_unique<int>(n);}

template<typename T>
void expect_val_eq(int n, const T& value) {
  EXPECT_EQ(val<T>(42), value);
}

template<>
void expect_val_eq<std::unique_ptr<int>>(int n, const std::unique_ptr<int>& value) {
  EXPECT_EQ(n, *value);
}

template<typename T>
struct printable {
  const T& value;
};

template<typename T>
std::ostream& operator<< (std::ostream& out, const printable<T>& printable) {
  return out << printable.value;
}

std::ostream& operator<< (std::ostream& out, const printable<std::unique_ptr<int>>& printable) {
  return out << *(printable.value);
}

} // anonymous namespace

template<typename T>
class FutureTests: public ::testing::Test {};
TYPED_TEST_CASE_P(FutureTests);

TYPED_TEST_P(FutureTests, default_constructed_is_invalid) {
  concurrency::future<TypeParam> future;
  EXPECT_FALSE(future.valid());
}

TYPED_TEST_P(FutureTests, obtained_from_promise_is_valid) {
  concurrency::promise<TypeParam> promise;
  auto future = promise.get_future();
  EXPECT_TRUE(future.valid());
}

TYPED_TEST_P(FutureTests, moved_to_constructor_is_invalid) {
  concurrency::promise<TypeParam> promise;
  auto future = promise.get_future();
  EXPECT_TRUE(future.valid());
  concurrency::future<TypeParam> another_future{std::move(future)};
  EXPECT_FALSE(future.valid());
  EXPECT_TRUE(another_future.valid());
}

TYPED_TEST_P(FutureTests, moved_to_assigment_to_invalid_is_invalid) {
  concurrency::promise<TypeParam> promise;
  auto future = promise.get_future();
  EXPECT_TRUE(future.valid());
  concurrency::future<TypeParam> another_future;
  EXPECT_FALSE(another_future.valid());
  another_future= std::move(future);
  EXPECT_FALSE(future.valid());
  EXPECT_TRUE(another_future.valid());
}

TYPED_TEST_P(FutureTests, moved_to_assigment_to_valid_is_invalid) {
  concurrency::promise<TypeParam> promise1;
  concurrency::promise<TypeParam> promise2;
  auto future1 = promise1.get_future();
  auto future2 = promise2.get_future();
  EXPECT_TRUE(future1.valid());
  EXPECT_TRUE(future2.valid());

  future2 = std::move(future1);
  EXPECT_TRUE(future2.valid());
  EXPECT_FALSE(future1.valid());
}

TYPED_TEST_P(FutureTests, share_of_invalid_is_invalid) {
  concurrency::future<TypeParam> future;
  ASSERT_FALSE(future.valid());
  auto shared = future.share();
  EXPECT_FALSE(shared.valid());
}

TYPED_TEST_P(FutureTests, share_invalidates_future) {
  concurrency::promise<TypeParam> promise;
  auto future = promise.get_future();
  ASSERT_TRUE(future.valid());
  auto shared = future.share();
  EXPECT_TRUE(shared.valid());
  EXPECT_FALSE(future.valid());
}

TYPED_TEST_P(FutureTests, get_on_invalid) {
  concurrency::future<TypeParam> future;
  ASSERT_FALSE(future.valid());
  EXPECT_FUTURE_ERROR(future.get(), concurrency::future_errc::no_state);
}

TYPED_TEST_P(FutureTests, retrieve_result) {
  auto future = set_value_in_other_thread(val<TypeParam>(42), 50ms);
  ASSERT_TRUE(future.valid());

  expect_val_eq(42, future.get());
  EXPECT_FALSE(future.valid());
}

TYPED_TEST_P(FutureTests, retrieve_exception) {
  auto future = set_error_in_other_thread<TypeParam>(std::runtime_error("test error"), 50ms);
  ASSERT_TRUE(future.valid());

  try {
    TypeParam unexpected_res = future.get();
    ADD_FAILURE() << "Value " << printable<TypeParam>{unexpected_res} << " was returned instead of exception";
  } catch(const std::runtime_error& err) {
    EXPECT_EQ("test error"s, err.what());
  } catch(const std::exception& err) {
    ADD_FAILURE() << "Unexpected exception: " << err.what();
  } catch(...) {
    ADD_FAILURE() << "Unexpected unknown exception type";
  }
  EXPECT_FALSE(future.valid());
}

TYPED_TEST_P(FutureTests, wait_on_invalid) {
  concurrency::future<TypeParam> future;
  ASSERT_FALSE(future.valid());
  EXPECT_FUTURE_ERROR(future.wait(), concurrency::future_errc::no_state);
}

TYPED_TEST_P(FutureTests, wait_for_on_invalid) {
  concurrency::future<TypeParam> future;
  ASSERT_FALSE(future.valid());
  EXPECT_FUTURE_ERROR(future.wait_for(5s), concurrency::future_errc::no_state);
}

TYPED_TEST_P(FutureTests, wait_until_on_invalid) {
  concurrency::future<TypeParam> future;
  ASSERT_FALSE(future.valid());
  EXPECT_FUTURE_ERROR(
    future.wait_until(sys_clock::now() + 5s),
    concurrency::future_errc::no_state
  );
}

TYPED_TEST_P(FutureTests, wait_on_ready_value) {
  concurrency::promise<TypeParam> promise;
  auto future = promise.get_future();
  promise.set_value(val<TypeParam>(42));
  ASSERT_TRUE(future.valid());

  EXPECT_NO_THROW(future.wait());
  EXPECT_TRUE(future.valid());

  EXPECT_EQ(concurrency::future_status::ready, future.wait_for(5s));
  EXPECT_TRUE(future.valid());

  EXPECT_EQ(concurrency::future_status::ready, future.wait_until(
    sys_clock::now() + 5s
  ));
  EXPECT_TRUE(future.valid());
}

TYPED_TEST_P(FutureTests, wait_on_ready_error) {
  concurrency::promise<TypeParam> promise;
  auto future = promise.get_future();
  promise.set_exception(std::make_exception_ptr(std::runtime_error("test error")));
  ASSERT_TRUE(future.valid());

  EXPECT_NO_THROW(future.wait());
  EXPECT_TRUE(future.valid());

  EXPECT_EQ(concurrency::future_status::ready, future.wait_for(5s));
  EXPECT_TRUE(future.valid());

  EXPECT_EQ(concurrency::future_status::ready, future.wait_until(
    sys_clock::now() + 5s
  ));
  EXPECT_TRUE(future.valid());
}

TYPED_TEST_P(FutureTests, wait_timeout) {
  concurrency::promise<TypeParam> promise;
  auto future = promise.get_future();
  ASSERT_TRUE(future.valid());

  EXPECT_EQ(concurrency::future_status::timeout, future.wait_for(10ms));
  EXPECT_TRUE(future.valid());

  EXPECT_EQ(concurrency::future_status::timeout, future.wait_until(
    hires_clock::now() + 10ms
  ));
  EXPECT_TRUE(future.valid());
}

TYPED_TEST_P(FutureTests, wait_awakes_on_value) {
  auto future = set_value_in_other_thread(val<TypeParam>(42), 50ms);
  ASSERT_TRUE(future.valid());

  EXPECT_NO_THROW(future.wait());
  EXPECT_TRUE(future.valid());
}

TYPED_TEST_P(FutureTests, wait_for_awakes_on_value) {
  auto future = set_value_in_other_thread(val<TypeParam>(42), 50ms);
  ASSERT_TRUE(future.valid());

  EXPECT_NO_THROW(future.wait_for(15s));
  EXPECT_TRUE(future.valid());
}

TYPED_TEST_P(FutureTests, wait_until_awakes_on_value) {
  auto future = set_value_in_other_thread(val<TypeParam>(42), 50ms);
  ASSERT_TRUE(future.valid());

  EXPECT_NO_THROW(future.wait_until(sys_clock::now() + 15s));
  EXPECT_TRUE(future.valid());
}

TYPED_TEST_P(FutureTests, wait_awakes_on_error) {
  auto future = set_error_in_other_thread<TypeParam>(std::runtime_error("test error"), 50ms);
  ASSERT_TRUE(future.valid());

  EXPECT_NO_THROW(future.wait());
  EXPECT_TRUE(future.valid());
}

TYPED_TEST_P(FutureTests, wait_for_awakes_on_error) {
  auto future = set_error_in_other_thread<TypeParam>(std::runtime_error("test error"), 50ms);
  ASSERT_TRUE(future.valid());

  EXPECT_NO_THROW(future.wait_for(15s));
  EXPECT_TRUE(future.valid());
}

TYPED_TEST_P(FutureTests, wait_until_awakes_on_error) {
  auto future = set_error_in_other_thread<TypeParam>(std::runtime_error("test error"), 50ms);
  ASSERT_TRUE(future.valid());

  EXPECT_NO_THROW(future.wait_until(sys_clock::now() + 15s));
  EXPECT_TRUE(future.valid());
}

REGISTER_TYPED_TEST_CASE_P(
  FutureTests,
  default_constructed_is_invalid,
  obtained_from_promise_is_valid,
  moved_to_constructor_is_invalid,
  moved_to_assigment_to_invalid_is_invalid,
  moved_to_assigment_to_valid_is_invalid,
  share_of_invalid_is_invalid,
  share_invalidates_future,
  get_on_invalid,
  retrieve_result,
  retrieve_exception,
  wait_on_invalid,
  wait_for_on_invalid,
  wait_until_on_invalid,
  wait_on_ready_value,
  wait_on_ready_error,
  wait_timeout,
  wait_awakes_on_value,
  wait_for_awakes_on_value,
  wait_until_awakes_on_value,
  wait_awakes_on_error,
  wait_for_awakes_on_error,
  wait_until_awakes_on_error
);

INSTANTIATE_TYPED_TEST_CASE_P(PrimitiveType, FutureTests, int);
INSTANTIATE_TYPED_TEST_CASE_P(CopyableType, FutureTests, std::string);
INSTANTIATE_TYPED_TEST_CASE_P(MoveableType, FutureTests, std::unique_ptr<int>);
