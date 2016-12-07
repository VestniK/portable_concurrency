#include <chrono>
#include <thread>

#include <gtest/gtest.h>

#include "concurrency/future"

#include "test_tools.h"

using namespace std::literals;
using sys_clock = std::chrono::system_clock;
using hires_clock = std::chrono::high_resolution_clock;

namespace {

template<typename T, typename R, typename P>
auto fullfill_value_in_other_thread(
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
auto fullfill_error_in_other_thread(
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

}

TEST(FutureTests, default_constructed_is_invalid) {
  concurrency::future<int> future;
  EXPECT_FALSE(future.valid());
}

TEST(FutureTests, obtained_from_promise_is_valid) {
  concurrency::promise<int> promise;
  auto future = promise.get_future();
  EXPECT_TRUE(future.valid());
}

TEST(FutureTests, moved_to_constructor_is_invalid) {
  concurrency::promise<int> promise;
  auto future = promise.get_future();
  EXPECT_TRUE(future.valid());
  concurrency::future<int> another_future{std::move(future)};
  EXPECT_FALSE(future.valid());
  EXPECT_TRUE(another_future.valid());
}

TEST(FutureTests, moved_to_assigment_to_invalid_is_invalid) {
  concurrency::promise<int> promise;
  auto future = promise.get_future();
  EXPECT_TRUE(future.valid());
  concurrency::future<int> another_future;
  EXPECT_FALSE(another_future.valid());
  another_future= std::move(future);
  EXPECT_FALSE(future.valid());
  EXPECT_TRUE(another_future.valid());
}

TEST(FutureTests, moved_to_assigment_to_valid_is_invalid) {
  concurrency::promise<int> promise1;
  concurrency::promise<int> promise2;
  auto future1 = promise1.get_future();
  auto future2 = promise2.get_future();
  EXPECT_TRUE(future1.valid());
  EXPECT_TRUE(future2.valid());

  future2 = std::move(future1);
  EXPECT_TRUE(future2.valid());
  EXPECT_FALSE(future1.valid());
}

TEST(FutureTests, share_of_invalid_is_invalid) {
  concurrency::future<int> future;
  ASSERT_FALSE(future.valid());
  auto shared = future.share();
  EXPECT_FALSE(shared.valid());
}

TEST(FutureTests, share_invalidates_future) {
  concurrency::promise<int> promise;
  auto future = promise.get_future();
  ASSERT_TRUE(future.valid());
  auto shared = future.share();
  EXPECT_TRUE(shared.valid());
  EXPECT_FALSE(future.valid());
}

TEST(FutureTests, get_on_invalid) {
  concurrency::future<int> future;
  ASSERT_FALSE(future.valid());
  EXPECT_FUTURE_ERROR(future.get(), concurrency::future_errc::no_state);
}

TEST(FutureTests, retrieve_result) {
  auto future = fullfill_value_in_other_thread(42, 50ms);
  ASSERT_TRUE(future.valid());

  EXPECT_EQ(42, future.get());
  EXPECT_FALSE(future.valid());
}

TEST(FutureTests, retrieve_exception) {
  auto future = fullfill_error_in_other_thread<int>(std::runtime_error("test error"), 50ms);
  ASSERT_TRUE(future.valid());

  try {
    int unexpected_res = future.get();
    ADD_FAILURE() << "Value " << unexpected_res << " was returned instead of exception";
  } catch(const std::runtime_error& err) {
    EXPECT_EQ("test error"s, err.what());
  } catch(const std::exception& err) {
    ADD_FAILURE() << "Unexpected exception: " << err.what();
  } catch(...) {
    ADD_FAILURE() << "Unexpected unknown exception type";
  }
  EXPECT_FALSE(future.valid());
}

TEST(FutureTests, wait_on_invalid) {
  concurrency::future<int> future;
  ASSERT_FALSE(future.valid());
  EXPECT_FUTURE_ERROR(future.wait(), concurrency::future_errc::no_state);
}

TEST(FutureTests, wait_for_on_invalid) {
  concurrency::future<int> future;
  ASSERT_FALSE(future.valid());
  EXPECT_FUTURE_ERROR(future.wait_for(5s), concurrency::future_errc::no_state);
}

TEST(FutureTests, wait_until_on_invalid) {
  concurrency::future<int> future;
  ASSERT_FALSE(future.valid());
  EXPECT_FUTURE_ERROR(
    future.wait_until(sys_clock::now() + 5s),
    concurrency::future_errc::no_state
  );
}

TEST(FutureTests, wait_on_ready_value) {
  concurrency::promise<int> promise;
  auto future = promise.get_future();
  promise.set_value(42);
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

TEST(FutureTests, wait_on_ready_error) {
  concurrency::promise<int> promise;
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

TEST(FutureTest, wait_timeout) {
  concurrency::promise<int> promise;
  auto future = promise.get_future();
  ASSERT_TRUE(future.valid());

  EXPECT_EQ(concurrency::future_status::timeout, future.wait_for(10ms));
  EXPECT_TRUE(future.valid());

  EXPECT_EQ(concurrency::future_status::timeout, future.wait_until(
    hires_clock::now() + 10ms
  ));
  EXPECT_TRUE(future.valid());
}

TEST(FutureTest, wait_awakes_on_value) {
  auto future = fullfill_value_in_other_thread(42, 50ms);
  ASSERT_TRUE(future.valid());

  EXPECT_NO_THROW(future.wait());
  EXPECT_TRUE(future.valid());
}

TEST(FutureTest, wait_for_awakes_on_value) {
  auto future = fullfill_value_in_other_thread(42, 50ms);
  ASSERT_TRUE(future.valid());

  EXPECT_NO_THROW(future.wait_for(15s));
  EXPECT_TRUE(future.valid());
}

TEST(FutureTest, wait_until_awakes_on_value) {
  auto future = fullfill_value_in_other_thread(42, 50ms);
  ASSERT_TRUE(future.valid());

  EXPECT_NO_THROW(future.wait_until(sys_clock::now() + 15s));
  EXPECT_TRUE(future.valid());
}

TEST(FutureTest, wait_awakes_on_error) {
  auto future = fullfill_error_in_other_thread<int>(std::runtime_error("test error"), 50ms);
  ASSERT_TRUE(future.valid());

  EXPECT_NO_THROW(future.wait());
  EXPECT_TRUE(future.valid());
}

TEST(FutureTest, wait_for_awakes_on_error) {
  auto future = fullfill_error_in_other_thread<int>(std::runtime_error("test error"), 50ms);
  ASSERT_TRUE(future.valid());

  EXPECT_NO_THROW(future.wait_for(15s));
  EXPECT_TRUE(future.valid());
}

TEST(FutureTest, wait_until_awakes_on_error) {
  auto future = fullfill_error_in_other_thread<int>(std::runtime_error("test error"), 50ms);
  ASSERT_TRUE(future.valid());

  EXPECT_NO_THROW(future.wait_until(sys_clock::now() + 15s));
  EXPECT_TRUE(future.valid());
}
