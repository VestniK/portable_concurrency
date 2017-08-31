#include <gtest/gtest.h>

#include <portable_concurrency/functional>

namespace {

namespace test {

namespace pc = portable_concurrency;

TEST(UniqueFunctionTest, default_constructed_is_empty) {
  pc::unique_function<void()> f;
  EXPECT_FALSE(f);
  ASSERT_THROW(f(), std::bad_function_call);
}

TEST(UniqueFunctionTest, nullptr_constructed_is_empty) {
  pc::unique_function<void()> f = nullptr;
  EXPECT_FALSE(f);
  ASSERT_THROW(f(), std::bad_function_call);
}

TEST(UniqueFunctionTest, null_func_ptr_constructed_is_empty) {
  using func_t = void(int);

  func_t* fptr = nullptr;
  pc::unique_function<void(int)> f = fptr;
  EXPECT_FALSE(f);
  ASSERT_THROW(f(5), std::bad_function_call);
}

TEST(UniqueFunctionTest, empty_std_func_constructed_is_empty) {
  std::function<int(std::string)> stdfunc;

  pc::unique_function<int(std::string)> f = stdfunc;
  EXPECT_FALSE(f);
  ASSERT_THROW(f("qwe"), std::bad_function_call);
}

TEST(UniqueFunctionTest, null_member_func_pointer_is_empty) {
  using mem_fn_ptr_t = decltype(&std::string::size);
  mem_fn_ptr_t null_mem_fn = nullptr;

  pc::unique_function<size_t(std::string)> f = null_mem_fn;
  EXPECT_FALSE(f);
  ASSERT_THROW(f("qwe"), std::bad_function_call);
}

} // namespace test

} // anonymous namespace
