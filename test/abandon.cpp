#include <future>

#include <gtest/gtest.h>

#include <portable_concurrency/future>

#include "test_tools.h"

using namespace std::literals;

namespace portable_concurrency {
namespace {
namespace test {

TEST(abandon_task_scheduled_by_async, fulfils_future_with_broken_promise_error) {
  pc::future<void> future = pc::async(null_executor, [] {});
  EXPECT_FUTURE_ERROR(future.get(), std::future_errc::broken_promise);
}

TEST(abandon_task_passed_to_future_next, fulfils_future_with_broken_promise_error) {
  pc::promise<int> promise;
  pc::future<int> future = promise.get_future();
  pc::future<std::string> cnt_f = future.next(null_executor, [](int val) { return std::to_string(val); });
  promise.set_value(42);
  EXPECT_FUTURE_ERROR(cnt_f.get(), std::future_errc::broken_promise);
}

TEST(abandon_task_passed_to_future_then, fulfils_future_with_broken_promise_error) {
  pc::promise<int> promise;
  pc::future<int> future = promise.get_future();
  pc::future<std::string> cnt_f = future.then(null_executor, [](pc::future<int> f) { return std::to_string(f.get()); });
  promise.set_value(42);
  EXPECT_FUTURE_ERROR(cnt_f.get(), std::future_errc::broken_promise);
}

TEST(return_invalid_future_from_future_then_continuation, fulfils_future_with_broken_promise_error) {
  pc::promise<int> promise;
  pc::future<int> future = promise.get_future();
  pc::future<std::string> cnt_f = future.then([](pc::future<int>) { return pc::future<std::string>{}; });
  promise.set_value(42);
  EXPECT_FUTURE_ERROR(cnt_f.get(), std::future_errc::broken_promise);
}

TEST(return_invalid_future_from_future_next_continuation, fulfils_future_with_broken_promise_error) {
  pc::promise<int> promise;
  pc::future<int> future = promise.get_future();
  pc::future<std::string> cnt_f = future.next([](int) { return pc::future<std::string>{}; });
  promise.set_value(42);
  EXPECT_FUTURE_ERROR(cnt_f.get(), std::future_errc::broken_promise);
}

TEST(premature_destruction_of_packaged_task, fulfils_future_with_broken_promise_error) {
  pc::future<size_t> future;
  {
    pc::packaged_task<size_t(size_t, const std::string&)> task{
        [](size_t a, const std::string& b) { return a + b.size(); }};
    future = task.get_future();
  }
  EXPECT_FUTURE_ERROR(future.get(), std::future_errc::broken_promise);
}

TEST(assignment_to_not_yet_called_packaged_task, fulfils_future_with_broken_promise_error) {
  pc::packaged_task<size_t(size_t, const std::string&)> task{
      [](size_t a, const std::string& b) { return a + b.size(); }};
  pc::future<size_t> future = task.get_future();
  task = {};
  EXPECT_FUTURE_ERROR(future.get(), std::future_errc::broken_promise);
}

} // namespace test
} // namespace
} // namespace portable_concurrency
