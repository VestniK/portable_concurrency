#include <string>
#include <memory>

#include <gtest/gtest.h>

#include "portable_concurrency/future"

#include "test_tools.h"
#include "test_helpers.h"

namespace {
namespace shared_future_continuation {

using namespace std::literals;

struct SharedFutureThen: future_test {
  pc::promise<int> promise;
  pc::shared_future<int> future = promise.get_future();
};

namespace tests {

template<typename T>
void exception_from_continuation() {
  pc::promise<T> p;
  auto f = p.get_future().share();

  pc::future<T> cont_f = f.then([](pc::shared_future<T>&& ready_f) -> T {
    EXPECT_TRUE(ready_f.is_ready());
    throw std::runtime_error("continuation error");
  });

  set_promise_value(p);
  EXPECT_TRUE(cont_f.is_ready());
  EXPECT_RUNTIME_ERROR(cont_f, "continuation error");
}

template<typename T>
void continuation_call() {
  pc::promise<T> p;
  auto f = p.get_future().share();

  pc::future<std::string> string_f = f.then([](pc::shared_future<T>&& ready_f) {
    EXPECT_TRUE(ready_f.is_ready());
    return to_string(ready_f.get());
  });

  p.set_value(some_value<T>());
  EXPECT_TRUE(string_f.is_ready());
  EXPECT_EQ(string_f.get(), to_string(some_value<T>()));
}

template<typename T>
void async_continuation_call() {
  auto f = set_value_in_other_thread<T>(25ms).share();

  pc::future<std::string> string_f = f.then([](pc::shared_future<T>&& ready_f) {
    EXPECT_TRUE(ready_f.is_ready());
    return to_string(ready_f.get());
  });

  EXPECT_EQ(string_f.get(), to_string(some_value<T>()));
}

template<typename T>
void ready_continuation_call() {
  auto f = pc::make_ready_future(some_value<T>()).share();

  pc::future<std::string> string_f = f.then([](pc::shared_future<T>&& ready_f) {
    EXPECT_TRUE(ready_f.is_ready());
    return to_string(ready_f.get());
  });

  EXPECT_EQ(string_f.get(), to_string(some_value<T>()));
}

template<typename T>
void void_continuation() {
  auto f = set_value_in_other_thread<T>(25ms).share();
  bool executed = false;

  pc::future<void> void_f = f.then(
    [&executed](pc::shared_future<T>&& ready_f) -> void {
      EXPECT_TRUE(ready_f.is_ready());
      ready_f.get();
      executed = true;
    }
  );

  EXPECT_NO_THROW(void_f.get());
  EXPECT_TRUE(executed);
}

template<typename T>
void ready_void_continuation() {
  auto f = pc::make_ready_future(some_value<T>()).share();
  bool executed = false;

  pc::future<void> void_f = f.then(
    [&executed](pc::shared_future<T>&& ready_f) -> void {
      EXPECT_TRUE(ready_f.is_ready());
      ready_f.get();
      executed = true;
    }
  );

  EXPECT_NO_THROW(void_f.get());
  EXPECT_TRUE(executed);
}

template<typename T>
void exception_to_continuation() {
  auto f = set_error_in_other_thread<T>(25ms, std::runtime_error("test error")).share();

  pc::future<std::string> string_f = f.then(
    [](pc::shared_future<T>&& ready_f) {
      EXPECT_TRUE(ready_f.is_ready());
      EXPECT_RUNTIME_ERROR(ready_f, "test error");
      return std::string{"Exception delivered"};
    }
  );

  EXPECT_EQ(string_f.get(), "Exception delivered");
}

template<typename T>
void exception_to_ready_continuation() {
  auto f = pc::make_exceptional_future<T>(std::runtime_error("test error")).share();

  pc::future<std::string> string_f = f.then(
    [](pc::shared_future<T>&& ready_f) {
      EXPECT_TRUE(ready_f.is_ready());
      EXPECT_RUNTIME_ERROR(ready_f, "test error");
      return std::string{"Exception delivered"};
    }
  );

  EXPECT_EQ(string_f.get(), "Exception delivered");
}

template<typename T>
void continuation_called_once() {
  pc::promise<T> p;
  auto sf1 = p.get_future().share();
  auto sf2 = sf1;

  unsigned call_count = 0;
  pc::future<std::string> cf = sf1.then([&call_count](pc::shared_future<T>&& rf) {
    ++call_count;
    EXPECT_TRUE(rf.is_ready());
    return to_string(rf.get());
  });
  EXPECT_EQ(0u, call_count);

  set_promise_value(p);
  EXPECT_EQ(1u, call_count);

  EXPECT_SOME_VALUE(sf1);
  EXPECT_EQ(1u, call_count);

  EXPECT_SOME_VALUE(sf2);
  EXPECT_EQ(1u, call_count);

  EXPECT_EQ(cf.get(), to_string(some_value<T>()));
  EXPECT_EQ(1u, call_count);
}

template<typename T>
void multiple_continuations() {
  auto sf1 = set_value_in_other_thread<T>(25ms).share();
  auto sf2 = sf1;

  std::atomic<unsigned> call_count1{0};
  std::atomic<unsigned> call_count2{0};

  pc::future<std::string> cf1 = sf1.then([&call_count1](pc::shared_future<T>&& rf) {
    ++call_count1;
    EXPECT_TRUE(rf.is_ready());
    return to_string(rf.get());
  });

  pc::future<std::string> cf2 = sf2.then([&call_count2](pc::shared_future<T>&& rf) {
    ++call_count2;
    EXPECT_TRUE(rf.is_ready());
    return to_string(rf.get());
  });

  EXPECT_EQ(call_count1.load(), 0u);
  EXPECT_EQ(call_count2.load(), 0u);

  EXPECT_EQ(cf1.get(), to_string(some_value<T>()));
  EXPECT_EQ(cf2.get(), to_string(some_value<T>()));

  EXPECT_EQ(call_count1.load(), 1u);
  EXPECT_EQ(call_count2.load(), 1u);
}

} // namespace tests

TEST_F(SharedFutureThen, future_remains_valid) {
  future.then([](pc::shared_future<int> f) {return std::to_string(f.get());});
  EXPECT_TRUE(future.valid());
}

TEST_F(SharedFutureThen, result_is_valid) {
  pc::future<std::string> cnt_f = future.then([](pc::shared_future<int> f) {return std::to_string(f.get());});
  EXPECT_TRUE(cnt_f.valid());
}

TEST_F(SharedFutureThen, result_is_not_ready_for_not_ready_source) {
  pc::future<std::string> cnt_f = future.then([](pc::shared_future<int> f) {return std::to_string(f.get());});
  EXPECT_FALSE(cnt_f.is_ready());
}

TEST_F(SharedFutureThen, result_is_ready_for_ready_source) {
  promise.set_value(42);
  pc::future<std::string> cnt_f = future.then([](pc::shared_future<int> f) {return std::to_string(f.get());});
  EXPECT_TRUE(cnt_f.is_ready());
}

TEST_F(SharedFutureThen, exception_from_continuation) {tests::exception_from_continuation<int>();}
TEST_F(SharedFutureThen, exception_to_continuation) {tests::exception_to_continuation<int>();}
TEST_F(SharedFutureThen, exception_to_ready_continuation) {tests::exception_to_ready_continuation<int>();}
TEST_F(SharedFutureThen, continuation_call) {tests::continuation_call<int>();}
TEST_F(SharedFutureThen, async_continuation_call) {tests::async_continuation_call<int>();}
TEST_F(SharedFutureThen, ready_continuation_call) {tests::ready_continuation_call<int>();}
TEST_F(SharedFutureThen, void_continuation) {tests::void_continuation<int>();}
TEST_F(SharedFutureThen, ready_void_continuation) {tests::ready_void_continuation<int>();}
TEST_F(SharedFutureThen, continuation_called_once) {tests::continuation_called_once<int>();}
TEST_F(SharedFutureThen, multiple_continuations) {tests::multiple_continuations<int>();}

TEST_F(SharedFutureThen, implicitly_unwrapps_futures) {
  pc::promise<void> inner_promise;
  auto cnt_f = future.then([inner_future = inner_promise.get_future()](pc::shared_future<int>) mutable {
    return std::move(inner_future);
  });
  static_assert(std::is_same<decltype(cnt_f), pc::future<void>>::value, "");
}

TEST_F(SharedFutureThen, unwrapped_future_is_not_ready_after_continuation_call) {
  pc::promise<void> inner_promise;
  pc::future<void> cnt_f = future.then([&](pc::shared_future<int>) {
    return inner_promise.get_future();
  });
  set_promise_value(promise);
  EXPECT_FALSE(cnt_f.is_ready());
}

TEST_F(SharedFutureThen, unwrapped_future_is_ready_after_continuation_result_becomes_ready) {
  pc::promise<void> inner_promise;
  pc::future<void> cnt_f = future.then([&](pc::shared_future<int>) {
    return inner_promise.get_future();
  });
  set_promise_value(promise);
  inner_promise.set_value();
  EXPECT_TRUE(cnt_f.is_ready());
}

TEST_F(SharedFutureThen, unwrapped_future_carries_broken_promise_for_invalid_result_of_continuation) {
  pc::future<std::string> cnt_f = future.then([](pc::shared_future<int>) {
    return pc::future<std::string>{};
  });
  set_promise_value(promise);
  EXPECT_FUTURE_ERROR(cnt_f.get(), std::future_errc::broken_promise);
}

TEST_F(SharedFutureThen, unwrapped_future_propagates_inner_future_error) {
  pc::promise<void> inner_promise;
  pc::future<void> cnt_f = future.then([&](pc::shared_future<int>) {
    return inner_promise.get_future();
  });
  set_promise_value(promise);
  inner_promise.set_exception(std::make_exception_ptr(std::runtime_error{"Ooups"}));
  EXPECT_RUNTIME_ERROR(cnt_f, "Ooups");
}

TEST_F(SharedFutureThen, exception_from_unwrapped_continuation_propagate_to_returned_future) {
  pc::future<std::unique_ptr<int>> cnt_f = future.then([](pc::shared_future<int>)
    -> pc::future<std::unique_ptr<int>>
  {
    throw std::runtime_error("Ooups");
  });
  set_promise_value(promise);
  EXPECT_RUNTIME_ERROR(cnt_f, "Ooups");
}

TEST_F(SharedFutureThen, run_continuation_on_specific_executor) {
  pc::future<std::thread::id> cnt_f = future.then(g_future_tests_env, [](pc::shared_future<int>) {
    return std::this_thread::get_id();
  });
  set_promise_value(promise);
  EXPECT_TRUE(g_future_tests_env->uses_thread(cnt_f.get()));
}

TEST_F(SharedFutureThen, run_unwrapped_continuation_on_specific_executor) {
  pc::future<std::thread::id> cnt_f = future.then(g_future_tests_env, [](pc::shared_future<int>) {
    return pc::make_ready_future(std::this_thread::get_id());
  });
  set_promise_value(promise);
  EXPECT_TRUE(g_future_tests_env->uses_thread(cnt_f.get()));
}

} // namespace shared_future_continuation
} // anonymous namespace
