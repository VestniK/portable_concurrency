#include <string>
#include <memory>

#include <gtest/gtest.h>

#include "concurrency/future"

#include "test_helpers.h"
#include "test_tools.h"

using namespace std::literals;

namespace {

template<typename T>
class PackagedTaskTest: public ::testing::Test {};
TYPED_TEST_CASE_P(PackagedTaskTest);

namespace tests {

template<typename T>
void default_constructed_is_invalid() {
  experimental::packaged_task<T()> task;
  EXPECT_FALSE(task.valid());
}

template<typename T>
void moved_to_constructor_is_invalid() {
  experimental::packaged_task<T()> task1(some_value<T>);
  EXPECT_TRUE(task1.valid());

  experimental::packaged_task<T()> task2(std::move(task1));
  EXPECT_FALSE(task1.valid());
  EXPECT_TRUE(task2.valid());
}

template<typename T>
void moved_to_assigment_is_invalid() {
  experimental::packaged_task<T()> task1(some_value<T>);
  experimental::packaged_task<T()> task2;
  EXPECT_TRUE(task1.valid());
  EXPECT_FALSE(task2.valid());

  task2 = std::move(task1);
  EXPECT_FALSE(task1.valid());
  EXPECT_TRUE(task2.valid());
}

template<typename T>
void swap_valid_task_with_invalid() {
  experimental::packaged_task<T()> task1(some_value<T>);
  experimental::packaged_task<T()> task2;
  EXPECT_TRUE(task1.valid());
  EXPECT_FALSE(task2.valid());

  task2.swap(task1);
  EXPECT_FALSE(task1.valid());
  EXPECT_TRUE(task2.valid());
}

template<typename T>
void get_task_future_twice() {
  experimental::packaged_task<T()> task(some_value<T>);
  experimental::future<T> f1, f2;
  EXPECT_NO_THROW(f1 = task.get_future());
  EXPECT_FUTURE_ERROR(
    f2 = task.get_future(),
    std::future_errc::future_already_retrieved
  );
}

template<typename T>
void successfull_call_makes_state_ready() {
  experimental::packaged_task<T()> task(some_value<T>);
  experimental::future<T> f = task.get_future();
  ASSERT_TRUE(f.valid());
  EXPECT_FALSE(f.is_ready());

  task();
  ASSERT_TRUE(f.valid());
  EXPECT_TRUE(f.is_ready());

  EXPECT_SOME_VALUE(f);
}

template<typename T>
void failed_call_makes_state_ready() {
  experimental::packaged_task<T()> task([]() -> T {
    throw std::runtime_error("operation failed");
  });
  experimental::future<T> f = task.get_future();
  ASSERT_TRUE(f.valid());
  EXPECT_FALSE(f.is_ready());

  EXPECT_NO_THROW(task());
  ASSERT_TRUE(f.valid());
  EXPECT_TRUE(f.is_ready());

  EXPECT_RUNTIME_ERROR(f, "operation failed");
}

template<typename T>
void swap_valid_tasks() {
  experimental::packaged_task<T()> task1(some_value<T>);
  experimental::packaged_task<T()> task2(some_value<T>);
  ASSERT_TRUE(task1.valid());
  ASSERT_TRUE(task2.valid());

  auto f1 = task1.get_future(), f2 = task2.get_future();
  ASSERT_TRUE(f1.valid());
  EXPECT_FALSE(f1.is_ready());
  ASSERT_TRUE(f2.valid());
  EXPECT_FALSE(f2.is_ready());

  task2.swap(task1);

  task1();
  EXPECT_FALSE(f1.is_ready());
  EXPECT_TRUE(f2.is_ready());

  task2();
  EXPECT_TRUE(f1.is_ready());
  EXPECT_TRUE(f2.is_ready());
}

template<typename T>
void call_task_twice() {
  experimental::packaged_task<T()> task(some_value<T>);
  ASSERT_TRUE(task.valid());

  ASSERT_NO_THROW(task());
  EXPECT_FUTURE_ERROR(task(), std::future_errc::promise_already_satisfied);
}

template<typename T>
void function_task() {
  experimental::packaged_task<T()> task(some_value<T>);
  auto f = task.get_future();
  g_future_tests_env->run_async(std::move(task));
  ASSERT_TRUE(f.valid());
  EXPECT_SOME_VALUE(f);
}

template<typename T>
void copyable_functor_task() {
  auto func = [timeout = 5ms]() -> T {
    std::this_thread::sleep_for(timeout);
    return some_value<T>();
  };
  static_assert(
    !std::is_function<decltype(func)>::value &&
    std::is_copy_constructible<decltype(func)>::value,
    "Test written incorrectly"
  );
  experimental::packaged_task<T()> task{func};
  auto f = task.get_future();
  g_future_tests_env->run_async(std::move(task));
  ASSERT_TRUE(f.valid());
  EXPECT_SOME_VALUE(f);
}

template<typename T>
void moveonly_functor_task() {
  auto timeout_ptr = std::make_unique<std::chrono::milliseconds>(5ms);
  auto func = [timeout = std::move(timeout_ptr)]() -> T {
    std::this_thread::sleep_for(*timeout);
    return some_value<T>();
  };
  static_assert(
    !std::is_function<decltype(func)>::value &&
    !std::is_copy_constructible<decltype(func)>::value &&
    std::is_move_constructible<decltype(func)>::value,
    "Test written incorrectly"
  );
  experimental::packaged_task<T()> task{std::move(func)};
  auto f = task.get_future();
  g_future_tests_env->run_async(std::move(task));
  ASSERT_TRUE(f.valid());
  EXPECT_SOME_VALUE(f);
}

template<typename T>
void one_param_task() {
  experimental::packaged_task<T(std::chrono::milliseconds)> task([](std::chrono::milliseconds tm) -> T {
    EXPECT_EQ(tm, 5ms);
    return some_value<T>();
  });
  auto f = task.get_future();
  g_future_tests_env->run_async(std::move(task), 5ms);
  ASSERT_TRUE(f.valid());
  EXPECT_SOME_VALUE(f);
}

template<typename T>
void two_param_task() {
  experimental::packaged_task<T(std::chrono::milliseconds, int)> task([](std::chrono::milliseconds tm, int n) -> T {
    EXPECT_EQ(tm, 1ms);
    EXPECT_EQ(n, 5);
    return some_value<T>();
  });
  auto f = task.get_future();
  g_future_tests_env->run_async(std::move(task), 1ms, 5);
  ASSERT_TRUE(f.valid());
  EXPECT_SOME_VALUE(f);
}

template<typename T>
void task_reset_simple() {
  auto task = experimental::packaged_task<T()>{some_value<T>};
  ASSERT_TRUE(task.valid());
  auto f = task.get_future();
  ASSERT_TRUE(f.valid());
  EXPECT_FALSE(f.is_ready());

  ASSERT_NO_THROW(task());
  EXPECT_TRUE(f.is_ready());
  EXPECT_SOME_VALUE(f);
  EXPECT_FALSE(f.valid());

  task.reset();
  ASSERT_NO_THROW(f = task.get_future());
  ASSERT_TRUE(f.valid());
  EXPECT_FALSE(f.is_ready());

  ASSERT_NO_THROW(task());
  EXPECT_TRUE(f.is_ready());
  EXPECT_SOME_VALUE(f);
  EXPECT_FALSE(f.valid());
}

template<typename T>
void task_reset_moveonly_func() {
  auto timeout_ptr = std::make_unique<std::chrono::milliseconds>(5ms);
  auto func = [timeout = std::move(timeout_ptr)]() -> T {
    std::this_thread::sleep_for(*timeout);
    return some_value<T>();
  };
  static_assert(
    !std::is_function<decltype(func)>::value &&
    !std::is_copy_constructible<decltype(func)>::value &&
    std::is_move_constructible<decltype(func)>::value,
    "Test written incorrectly"
  );

  auto task = experimental::packaged_task<T()>{std::move(func)};
  ASSERT_TRUE(task.valid());
  auto f = task.get_future();
  ASSERT_TRUE(f.valid());
  EXPECT_FALSE(f.is_ready());

  ASSERT_NO_THROW(task());
  EXPECT_TRUE(f.is_ready());
  EXPECT_SOME_VALUE(f);
  EXPECT_FALSE(f.valid());

  task.reset();
  ASSERT_NO_THROW(f = task.get_future());
  ASSERT_TRUE(f.valid());
  EXPECT_FALSE(f.is_ready());

  ASSERT_NO_THROW(task());
  EXPECT_TRUE(f.is_ready());
  EXPECT_SOME_VALUE(f);
  EXPECT_FALSE(f.valid());
}

} // namespace tests

TYPED_TEST_P(PackagedTaskTest, default_constructed_is_invalid) {tests::default_constructed_is_invalid<TypeParam>();}
TYPED_TEST_P(PackagedTaskTest, moved_to_constructor_is_invalid) {tests::moved_to_constructor_is_invalid<TypeParam>();}
TYPED_TEST_P(PackagedTaskTest, moved_to_assigment_is_invalid) {tests::moved_to_assigment_is_invalid<TypeParam>();}
TYPED_TEST_P(PackagedTaskTest, swap_valid_task_with_invalid) {tests::swap_valid_task_with_invalid<TypeParam>();}
TYPED_TEST_P(PackagedTaskTest, get_task_future_twice) {tests::get_task_future_twice<TypeParam>();}
TYPED_TEST_P(PackagedTaskTest, successfull_call_makes_state_ready) {tests::successfull_call_makes_state_ready<TypeParam>();}
TYPED_TEST_P(PackagedTaskTest, failed_call_makes_state_ready) {tests::failed_call_makes_state_ready<TypeParam>();}
TYPED_TEST_P(PackagedTaskTest, swap_valid_tasks) {tests::swap_valid_tasks<TypeParam>();}
TYPED_TEST_P(PackagedTaskTest, call_task_twice) {tests::call_task_twice<TypeParam>();}
TYPED_TEST_P(PackagedTaskTest, function_task) {tests::function_task<TypeParam>();}
TYPED_TEST_P(PackagedTaskTest, copyable_functor_task) {tests::copyable_functor_task<TypeParam>();}
TYPED_TEST_P(PackagedTaskTest, moveonly_functor_task) {tests::moveonly_functor_task<TypeParam>();}
TYPED_TEST_P(PackagedTaskTest, one_param_task) {tests::one_param_task<TypeParam>();}
TYPED_TEST_P(PackagedTaskTest, two_param_task) {tests::two_param_task<TypeParam>();}
TYPED_TEST_P(PackagedTaskTest, task_reset_simple) {tests::task_reset_simple<TypeParam>();}
TYPED_TEST_P(PackagedTaskTest, task_reset_moveonly_func) {tests::task_reset_moveonly_func<TypeParam>();}
REGISTER_TYPED_TEST_CASE_P(
  PackagedTaskTest,
  default_constructed_is_invalid,
  moved_to_constructor_is_invalid,
  moved_to_assigment_is_invalid,
  swap_valid_task_with_invalid,
  get_task_future_twice,
  successfull_call_makes_state_ready,
  failed_call_makes_state_ready,
  swap_valid_tasks,
  call_task_twice,
  function_task,
  copyable_functor_task,
  moveonly_functor_task,
  one_param_task,
  two_param_task,
  task_reset_simple,
  task_reset_moveonly_func
);

INSTANTIATE_TYPED_TEST_CASE_P(VoidType, PackagedTaskTest, void);
INSTANTIATE_TYPED_TEST_CASE_P(PrimitiveType, PackagedTaskTest, int);
INSTANTIATE_TYPED_TEST_CASE_P(CopyableType, PackagedTaskTest, std::string);
INSTANTIATE_TYPED_TEST_CASE_P(MoveableType, PackagedTaskTest, std::unique_ptr<int>);
INSTANTIATE_TYPED_TEST_CASE_P(ReferenceType, PackagedTaskTest, future_tests_env&);

} // anonymous namespace
