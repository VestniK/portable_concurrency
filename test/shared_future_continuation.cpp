#include <string>
#include <memory>

#include <gtest/gtest.h>

#include "portable_concurrency/future"

#include "test_tools.h"
#include "test_helpers.h"

namespace {
namespace shared_future_continuation {

using namespace std::literals;

template<typename T>
class SharedFutureContinuationsTest: public ::testing::Test {};
TYPED_TEST_CASE_P(SharedFutureContinuationsTest);

namespace tests {

template<typename T>
void exception_from_continuation() {
  pc::promise<T> p;
  auto f = p.get_future().share();
  ASSERT_TRUE(f.valid());

  pc::future<T> cont_f = f.then([](pc::shared_future<T>&& ready_f) -> T {
    EXPECT_TRUE(ready_f.is_ready());
    throw std::runtime_error("continuation error");
  });
  EXPECT_TRUE(f.valid());
  EXPECT_TRUE(cont_f.valid());
  EXPECT_FALSE(cont_f.is_ready());

  set_promise_value(p);
  EXPECT_TRUE(cont_f.is_ready());
  EXPECT_RUNTIME_ERROR(cont_f, "continuation error");
}

template<typename T>
void continuation_call() {
  pc::promise<T> p;
  auto f = p.get_future().share();
  ASSERT_TRUE(f.valid());

  pc::future<std::string> string_f = f.then([](pc::shared_future<T>&& ready_f) {
    EXPECT_TRUE(ready_f.is_ready());
    return to_string(ready_f.get());
  });
  EXPECT_TRUE(f.valid());
  EXPECT_TRUE(string_f.valid());
  EXPECT_FALSE(string_f.is_ready());

  p.set_value(some_value<T>());
  EXPECT_TRUE(string_f.is_ready());
  EXPECT_EQ(string_f.get(), to_string(some_value<T>()));
}

template<>
void continuation_call<void>() {
  pc::promise<void> p;
  auto f = p.get_future().share();
  ASSERT_TRUE(f.valid());

  pc::future<std::string> string_f = f.then([](pc::shared_future<void>&& ready_f) {
    EXPECT_TRUE(ready_f.is_ready());
    ready_f.get();
    return "void value"s;
  });
  EXPECT_TRUE(f.valid());
  EXPECT_TRUE(string_f.valid());
  EXPECT_FALSE(string_f.is_ready());

  p.set_value();
  EXPECT_TRUE(string_f.is_ready());
  EXPECT_EQ(string_f.get(), "void value"s);
}

template<typename T>
void async_continuation_call() {
  auto f = set_value_in_other_thread<T>(25ms).share();
  ASSERT_TRUE(f.valid());

  pc::future<std::string> string_f = f.then([](pc::shared_future<T>&& ready_f) {
    EXPECT_TRUE(ready_f.is_ready());
    return to_string(ready_f.get());
  });
  EXPECT_TRUE(f.valid());
  EXPECT_TRUE(string_f.valid());
  EXPECT_FALSE(string_f.is_ready());

  EXPECT_EQ(string_f.get(), to_string(some_value<T>()));
}

template<>
void async_continuation_call<void>() {
  auto f = set_value_in_other_thread<void>(25ms).share();
  ASSERT_TRUE(f.valid());

  pc::future<std::string> string_f = f.then([](pc::shared_future<void>&& ready_f) {
    EXPECT_TRUE(ready_f.is_ready());
    ready_f.get();
    return "void value"s;
  });
  EXPECT_TRUE(f.valid());
  EXPECT_TRUE(string_f.valid());
  EXPECT_FALSE(string_f.is_ready());

  EXPECT_EQ(string_f.get(), "void value"s);
}

template<typename T>
void ready_continuation_call() {
  auto f = pc::make_ready_future(some_value<T>()).share();

  pc::future<std::string> string_f = f.then([](pc::shared_future<T>&& ready_f) {
    EXPECT_TRUE(ready_f.is_ready());
    return to_string(ready_f.get());
  });
  EXPECT_TRUE(f.valid());
  EXPECT_TRUE(string_f.valid());
  EXPECT_TRUE(string_f.is_ready());

  EXPECT_EQ(string_f.get(), to_string(some_value<T>()));
}

template<>
void ready_continuation_call<future_tests_env&>() {
  auto f = pc::make_ready_future(std::ref(some_value<future_tests_env&>())).share();

  pc::future<std::string> string_f = f.then(
    [](pc::shared_future<future_tests_env&>&& ready_f) {
      EXPECT_TRUE(ready_f.is_ready());
      return to_string(ready_f.get());
    }
  );
  EXPECT_TRUE(f.valid());
  EXPECT_TRUE(string_f.valid());
  EXPECT_TRUE(string_f.is_ready());

  EXPECT_EQ(string_f.get(), to_string(some_value<future_tests_env&>()));
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
  EXPECT_TRUE(f.valid());
  EXPECT_TRUE(void_f.valid());
  EXPECT_FALSE(void_f.is_ready());

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
  EXPECT_TRUE(f.valid());
  EXPECT_TRUE(void_f.valid());
  EXPECT_TRUE(void_f.is_ready());

  EXPECT_NO_THROW(void_f.get());
  EXPECT_TRUE(executed);
}

template<>
void ready_void_continuation<future_tests_env&>() {
  auto f = pc::make_ready_future(std::ref(some_value<future_tests_env&>())).share();
  bool executed = false;

  pc::future<void> void_f = f.then(
    [&executed](pc::shared_future<future_tests_env&>&& ready_f) -> void {
      EXPECT_TRUE(ready_f.is_ready());
      ready_f.get();
      executed = true;
    }
  );
  EXPECT_TRUE(f.valid());
  EXPECT_TRUE(void_f.valid());
  EXPECT_TRUE(void_f.is_ready());

  EXPECT_NO_THROW(void_f.get());
  EXPECT_TRUE(executed);
}

template<>
void ready_void_continuation<void>() {
  auto f = pc::make_ready_future().share();
  bool executed = false;

  pc::future<void> void_f = f.then(
    [&executed](pc::shared_future<void>&& ready_f) -> void {
      EXPECT_TRUE(ready_f.is_ready());
      ready_f.get();
      executed = true;
    }
  );
  EXPECT_TRUE(f.valid());
  EXPECT_TRUE(void_f.valid());
  EXPECT_TRUE(void_f.is_ready());

  EXPECT_NO_THROW(void_f.get());
  EXPECT_TRUE(executed);
}

template<>
void ready_continuation_call<void>() {
  auto f = pc::make_ready_future().share();

  pc::future<std::string> string_f = f.then(
    [](pc::shared_future<void>&& ready_f) {
      EXPECT_TRUE(ready_f.is_ready());
      ready_f.get();
      return "void value"s;
    }
  );
  EXPECT_TRUE(f.valid());
  EXPECT_TRUE(string_f.valid());
  EXPECT_TRUE(string_f.is_ready());

  EXPECT_EQ(string_f.get(), "void value"s);
}

template<typename T>
void exception_to_continuation() {
  auto f = set_error_in_other_thread<T>(25ms, std::runtime_error("test error")).share();

  pc::future<std::string> string_f = f.then(
    [](pc::shared_future<T>&& ready_f) {
      EXPECT_TRUE(ready_f.is_ready());
      EXPECT_RUNTIME_ERROR(ready_f, "test error");
      return "Exception delivered"s;
    }
  );
  EXPECT_TRUE(f.valid());
  EXPECT_TRUE(string_f.valid());
  EXPECT_FALSE(string_f.is_ready());

  EXPECT_EQ(string_f.get(), "Exception delivered"s);
}

template<typename T>
void exception_to_ready_continuation() {
  auto f = pc::make_exceptional_future<T>(std::runtime_error("test error")).share();

  pc::future<std::string> string_f = f.then(
    [](pc::shared_future<T>&& ready_f) {
      EXPECT_TRUE(ready_f.is_ready());
      EXPECT_RUNTIME_ERROR(ready_f, "test error");
      return "Exception delivered"s;
    }
  );
  EXPECT_TRUE(f.valid());
  EXPECT_TRUE(string_f.valid());
  EXPECT_TRUE(string_f.is_ready());

  EXPECT_EQ(string_f.get(), "Exception delivered"s);
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
  ASSERT_TRUE(cf.valid());
  ASSERT_TRUE(sf1.valid());
  ASSERT_TRUE(sf2.valid());
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

template<>
void continuation_called_once<void>() {
  pc::promise<void> p;
  auto sf1 = p.get_future().share();
  auto sf2 = sf1;

  unsigned call_count = 0;
  pc::future<std::string> cf = sf1.then([&call_count](pc::shared_future<void>&& rf) {
    ++call_count;
    EXPECT_TRUE(rf.is_ready());
    rf.get();
    return "void value"s;
  });
  ASSERT_TRUE(cf.valid());
  ASSERT_TRUE(sf1.valid());
  ASSERT_TRUE(sf2.valid());
  EXPECT_EQ(0u, call_count);

  set_promise_value(p);
  EXPECT_EQ(1u, call_count);

  EXPECT_SOME_VALUE(sf1);
  EXPECT_EQ(1u, call_count);

  EXPECT_SOME_VALUE(sf2);
  EXPECT_EQ(1u, call_count);

  EXPECT_EQ(cf.get(), "void value"s);
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
  ASSERT_TRUE(cf1.valid());

  pc::future<std::string> cf2 = sf2.then([&call_count2](pc::shared_future<T>&& rf) {
    ++call_count2;
    EXPECT_TRUE(rf.is_ready());
    return to_string(rf.get());
  });
  ASSERT_TRUE(cf2.valid());

  EXPECT_EQ(call_count1.load(), 0u);
  EXPECT_EQ(call_count2.load(), 0u);

  EXPECT_EQ(cf1.get(), to_string(some_value<T>()));
  EXPECT_EQ(cf2.get(), to_string(some_value<T>()));

  EXPECT_EQ(call_count1.load(), 1u);
  EXPECT_EQ(call_count2.load(), 1u);
}

template<>
void multiple_continuations<void>() {
  auto sf1 = set_value_in_other_thread<void>(25ms).share();
  auto sf2 = sf1;

  std::atomic<unsigned> call_count1{0};
  std::atomic<unsigned> call_count2{0};

  pc::future<std::string> cf1 = sf1.then([&call_count1](pc::shared_future<void>&& rf) {
    ++call_count1;
    EXPECT_TRUE(rf.is_ready());
    rf.get();
    return "void one"s;
  });
  ASSERT_TRUE(cf1.valid());

  pc::future<std::string> cf2 = sf2.then([&call_count2](pc::shared_future<void>&& rf) {
    ++call_count2;
    EXPECT_TRUE(rf.is_ready());
    rf.get();
    return "void two"s;
  });
  ASSERT_TRUE(cf2.valid());

  EXPECT_EQ(call_count1.load(), 0u);
  EXPECT_EQ(call_count2.load(), 0u);

  EXPECT_EQ(cf1.get(), "void one"s);
  EXPECT_EQ(cf2.get(), "void two"s);

  EXPECT_EQ(call_count1.load(), 1u);
  EXPECT_EQ(call_count2.load(), 1u);
}

} // namespace tests

TYPED_TEST_P(SharedFutureContinuationsTest, exception_from_continuation) {tests::exception_from_continuation<TypeParam>();}
TYPED_TEST_P(SharedFutureContinuationsTest, exception_to_continuation) {tests::exception_to_continuation<TypeParam>();}
TYPED_TEST_P(SharedFutureContinuationsTest, exception_to_ready_continuation) {tests::exception_to_ready_continuation<TypeParam>();}
TYPED_TEST_P(SharedFutureContinuationsTest, continuation_call) {tests::continuation_call<TypeParam>();}
TYPED_TEST_P(SharedFutureContinuationsTest, async_continuation_call) {tests::async_continuation_call<TypeParam>();}
TYPED_TEST_P(SharedFutureContinuationsTest, ready_continuation_call) {tests::ready_continuation_call<TypeParam>();}
TYPED_TEST_P(SharedFutureContinuationsTest, void_continuation) {tests::void_continuation<TypeParam>();}
TYPED_TEST_P(SharedFutureContinuationsTest, ready_void_continuation) {tests::ready_void_continuation<TypeParam>();}
TYPED_TEST_P(SharedFutureContinuationsTest, continuation_called_once) {tests::continuation_called_once<TypeParam>();}
TYPED_TEST_P(SharedFutureContinuationsTest, multiple_continuations) {tests::multiple_continuations<TypeParam>();}
REGISTER_TYPED_TEST_CASE_P(
  SharedFutureContinuationsTest,
  exception_from_continuation,
  exception_to_continuation,
  exception_to_ready_continuation,
  continuation_call,
  async_continuation_call,
  ready_continuation_call,
  void_continuation,
  ready_void_continuation,
  continuation_called_once,
  multiple_continuations
);

INSTANTIATE_TYPED_TEST_CASE_P(VoidType, SharedFutureContinuationsTest, void);
INSTANTIATE_TYPED_TEST_CASE_P(PrimitiveType, SharedFutureContinuationsTest, int);
INSTANTIATE_TYPED_TEST_CASE_P(CopyableType, SharedFutureContinuationsTest, std::string);
INSTANTIATE_TYPED_TEST_CASE_P(MoveableType, SharedFutureContinuationsTest, std::unique_ptr<int>);
INSTANTIATE_TYPED_TEST_CASE_P(ReferenceType, SharedFutureContinuationsTest, future_tests_env&);

} // namespace shared_future_continuation
} // anonymous namespace
