#include <string>
#include <memory>

#include <gtest/gtest.h>

#include "portable_concurrency/future"

#include "test_tools.h"
#include "test_helpers.h"

namespace {

using namespace std::literals;

template<typename T>
class UnwrapConstructorTest: public ::testing::Test {};
TYPED_TEST_CASE_P(UnwrapConstructorTest);

namespace tests {

template<typename T>
void future_async_async() {
  pc::promise<pc::future<T>> p;
  auto f = p.get_future();
  g_future_tests_env->run_async([](pc::promise<pc::future<T>>&& p) {
    std::this_thread::sleep_for(15ms);
    p.set_value(set_value_in_other_thread<T>(15ms));
  }, std::move(p));
  ASSERT_TRUE(f.valid());

  pc::future<T> unwrapped_f(std::move(f));
  EXPECT_FALSE(f.valid());
  EXPECT_TRUE(unwrapped_f.valid());
  EXPECT_FALSE(unwrapped_f.is_ready());
  EXPECT_SOME_VALUE(unwrapped_f);
}

template<typename T>
void future_async_ready() {
  pc::promise<pc::future<T>> p;
  auto f = p.get_future();
  g_future_tests_env->run_async([](pc::promise<pc::future<T>>&& p) {
    std::this_thread::sleep_for(15ms);
    p.set_value(make_some_ready_future<T>());
  }, std::move(p));
  ASSERT_TRUE(f.valid());

  pc::future<T> unwrapped_f(std::move(f));
  EXPECT_FALSE(f.valid());
  EXPECT_TRUE(unwrapped_f.valid());
  EXPECT_FALSE(unwrapped_f.is_ready());
  EXPECT_SOME_VALUE(unwrapped_f);
}

template<typename T>
void future_async_invalid() {
  pc::promise<pc::future<T>> p;
  auto f = p.get_future();
  g_future_tests_env->run_async([](pc::promise<pc::future<T>>&& p) {
    std::this_thread::sleep_for(15ms);
    p.set_value(pc::future<T>{});
  }, std::move(p));
  ASSERT_TRUE(f.valid());

  pc::future<T> unwrapped_f(std::move(f));
  EXPECT_FALSE(f.valid());
  EXPECT_TRUE(unwrapped_f.valid());
  EXPECT_FALSE(unwrapped_f.is_ready());
  EXPECT_FUTURE_ERROR(unwrapped_f.get(), std::future_errc::broken_promise);
}

template<typename T>
void future_ready_async() {
  pc::future<pc::future<T>> f = pc::make_ready_future(
    set_value_in_other_thread<T>(15ms)
  );
  ASSERT_TRUE(f.valid());
  ASSERT_TRUE(f.is_ready());

  pc::future<T> unwrapped_f(std::move(f));
  EXPECT_FALSE(f.valid());
  EXPECT_TRUE(unwrapped_f.valid());
  EXPECT_FALSE(unwrapped_f.is_ready());
  EXPECT_SOME_VALUE(unwrapped_f);
}

template<typename T>
void future_ready_ready() {
  pc::future<pc::future<T>> f = pc::make_ready_future(
    make_some_ready_future<T>()
  );
  ASSERT_TRUE(f.valid());
  ASSERT_TRUE(f.is_ready());

  pc::future<T> unwrapped_f(std::move(f));
  EXPECT_FALSE(f.valid());
  EXPECT_TRUE(unwrapped_f.valid());
  EXPECT_TRUE(unwrapped_f.is_ready());
  EXPECT_SOME_VALUE(unwrapped_f);
}

template<typename T>
void future_ready_invalid() {
  auto f = pc::make_ready_future(pc::future<T>{});
  ASSERT_TRUE(f.valid());

  pc::future<T> unwrapped_f(std::move(f));
  EXPECT_FALSE(f.valid());
  EXPECT_TRUE(unwrapped_f.valid());
  EXPECT_TRUE(unwrapped_f.is_ready());
  EXPECT_FUTURE_ERROR(unwrapped_f.get(), std::future_errc::broken_promise);
}

template<typename T>
void future_async_async_error() {
  pc::promise<pc::future<T>> p;
  auto f = p.get_future();
  g_future_tests_env->run_async([](pc::promise<pc::future<T>>&& p) {
    std::this_thread::sleep_for(15ms);
    p.set_value(set_error_in_other_thread<T>(15ms, std::runtime_error("test error")));
  }, std::move(p));
  ASSERT_TRUE(f.valid());

  pc::future<T> unwrapped_f(std::move(f));
  EXPECT_FALSE(f.valid());
  EXPECT_TRUE(unwrapped_f.valid());
  EXPECT_FALSE(unwrapped_f.is_ready());
  EXPECT_RUNTIME_ERROR(unwrapped_f, "test error");
}

template<typename T>
void future_async_ready_error() {
  pc::promise<pc::future<T>> p;
  auto f = p.get_future();
  g_future_tests_env->run_async([](pc::promise<pc::future<T>>&& p) {
    std::this_thread::sleep_for(15ms);
    p.set_value(pc::make_exceptional_future<T>(std::runtime_error("test error")));
  }, std::move(p));
  ASSERT_TRUE(f.valid());

  pc::future<T> unwrapped_f(std::move(f));
  EXPECT_FALSE(f.valid());
  EXPECT_TRUE(unwrapped_f.valid());
  EXPECT_FALSE(unwrapped_f.is_ready());
  EXPECT_RUNTIME_ERROR(unwrapped_f, "test error");
}

template<typename T>
void future_ready_async_error() {
  pc::future<pc::future<T>> f = pc::make_ready_future(
    set_error_in_other_thread<T>(15ms, std::runtime_error("test error"))
  );
  ASSERT_TRUE(f.valid());

  pc::future<T> unwrapped_f(std::move(f));
  EXPECT_FALSE(f.valid());
  EXPECT_TRUE(unwrapped_f.valid());
  EXPECT_FALSE(unwrapped_f.is_ready());
  EXPECT_RUNTIME_ERROR(unwrapped_f, "test error");
}

template<typename T>
void future_ready_ready_error() {
  pc::future<pc::future<T>> f = pc::make_ready_future(
    pc::make_exceptional_future<T>(std::runtime_error("test error"))
  );
  ASSERT_TRUE(f.valid());

  pc::future<T> unwrapped_f(std::move(f));
  EXPECT_FALSE(f.valid());
  EXPECT_TRUE(unwrapped_f.valid());
  EXPECT_TRUE(unwrapped_f.is_ready());
  EXPECT_RUNTIME_ERROR(unwrapped_f, "test error");
}

template<typename T>
void future_ready_error() {
  pc::future<pc::future<T>> f =
    pc::make_exceptional_future<pc::future<T>>(std::runtime_error("test error"))
  ;
  ASSERT_TRUE(f.valid());

  pc::future<T> unwrapped_f(std::move(f));
  EXPECT_FALSE(f.valid());
  EXPECT_TRUE(unwrapped_f.valid());
  EXPECT_TRUE(unwrapped_f.is_ready());
  EXPECT_RUNTIME_ERROR(unwrapped_f, "test error");
}

template<typename T>
void wait_before_and_after_unwrap() {
  pc::promise<pc::future<T>> wrap_p;
  pc::promise<T> p;

  auto wrap_f = wrap_p.get_future();
  EXPECT_EQ(wrap_f.wait_for(100us), std::future_status::timeout);

  pc::future<T> f = std::move(wrap_f);
  EXPECT_EQ(f.wait_for(100us), std::future_status::timeout);

  wrap_p.set_value(p.get_future());
  EXPECT_EQ(f.wait_for(100us), std::future_status::timeout);

  set_promise_value(p);
  EXPECT_EQ(f.wait_for(100us), std::future_status::ready);
}

} // namespace tests

TYPED_TEST_P(UnwrapConstructorTest, future_async_async) {tests::future_async_async<TypeParam>();}
TYPED_TEST_P(UnwrapConstructorTest, future_async_ready) {tests::future_async_ready<TypeParam>();}
TYPED_TEST_P(UnwrapConstructorTest, future_async_invalid) {tests::future_async_invalid<TypeParam>();}
TYPED_TEST_P(UnwrapConstructorTest, future_ready_async) {tests::future_ready_async<TypeParam>();}
TYPED_TEST_P(UnwrapConstructorTest, future_ready_ready) {tests::future_ready_ready<TypeParam>();}
TYPED_TEST_P(UnwrapConstructorTest, future_ready_invalid) {tests::future_ready_invalid<TypeParam>();}
TYPED_TEST_P(UnwrapConstructorTest, future_async_async_error) {tests::future_async_async_error<TypeParam>();}
TYPED_TEST_P(UnwrapConstructorTest, future_async_ready_error) {tests::future_async_ready_error<TypeParam>();}
TYPED_TEST_P(UnwrapConstructorTest, future_ready_async_error) {tests::future_ready_async_error<TypeParam>();}
TYPED_TEST_P(UnwrapConstructorTest, future_ready_ready_error) {tests::future_ready_ready_error<TypeParam>();}
TYPED_TEST_P(UnwrapConstructorTest, future_ready_error) {tests::future_ready_error<TypeParam>();}
TYPED_TEST_P(UnwrapConstructorTest, wait_before_and_after_unwrap) {tests::wait_before_and_after_unwrap<TypeParam>();}
REGISTER_TYPED_TEST_CASE_P(
  UnwrapConstructorTest,
  future_async_async,
  future_async_ready,
  future_async_invalid,
  future_ready_async,
  future_ready_ready,
  future_ready_invalid,
  future_async_async_error,
  future_async_ready_error,
  future_ready_async_error,
  future_ready_ready_error,
  future_ready_error,
  wait_before_and_after_unwrap
);

INSTANTIATE_TYPED_TEST_CASE_P(VoidType, UnwrapConstructorTest, void);
INSTANTIATE_TYPED_TEST_CASE_P(PrimitiveType, UnwrapConstructorTest, int);
INSTANTIATE_TYPED_TEST_CASE_P(CopyableType, UnwrapConstructorTest, std::string);
INSTANTIATE_TYPED_TEST_CASE_P(MoveableType, UnwrapConstructorTest, std::unique_ptr<int>);
INSTANTIATE_TYPED_TEST_CASE_P(ReferenceType, UnwrapConstructorTest, future_tests_env&);

} // anonymous namespace
