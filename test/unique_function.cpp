#include <cmath>
#include <cstring>

#include <gtest/gtest.h>

#include <portable_concurrency/functional>

namespace {

namespace test {

namespace pc = portable_concurrency;

struct point {
  int x;
  int y;

  int dist_from_center() const {return std::sqrt(x*x + y*y);}
};

struct big {
  std::uint64_t u0;
  std::uint64_t u1;
  std::uint64_t u2;
  std::uint64_t u3;
  std::uint64_t u4;
  std::uint64_t u5;
};

TEST(UniqueFunction, default_constructed_is_empty) {
  pc::unique_function<void()> f;
  EXPECT_FALSE(f);
  ASSERT_THROW(f(), std::bad_function_call);
}

TEST(UniqueFunction, nullptr_constructed_is_empty) {
  pc::unique_function<void()> f = nullptr;
  EXPECT_FALSE(f);
  ASSERT_THROW(f(), std::bad_function_call);
}

TEST(UniqueFunction, null_func_ptr_constructed_is_empty) {
  using func_t = void(int);

  func_t* fptr = nullptr;
  pc::unique_function<void(int)> f = fptr;
  EXPECT_FALSE(f);
  ASSERT_THROW(f(5), std::bad_function_call);
}

TEST(UniqueFunction, empty_std_func_constructed_is_empty) {
  std::function<int(std::string)> stdfunc;

  pc::unique_function<int(std::string)> f = stdfunc;
  EXPECT_FALSE(f);
  ASSERT_THROW(f("qwe"), std::bad_function_call);
}

TEST(UniqueFunction, null_member_func_pointer_is_empty) {
  using mem_fn_ptr_t = decltype(&std::string::size);
  mem_fn_ptr_t null_mem_fn = nullptr;

  pc::unique_function<size_t(std::string)> f = null_mem_fn;
  EXPECT_FALSE(f);
  ASSERT_THROW(f("qwe"), std::bad_function_call);
}

TEST(UniqueFunction, null_member_obj_ptr_constructed_is_empty) {
  decltype(&point::x) null_xptr = nullptr;

  pc::unique_function<int(point)> f = null_xptr;
  EXPECT_FALSE(f);
  ASSERT_THROW(f(point{100, 500}), std::bad_function_call);
}

TEST(UniqueFunction, raw_function_call) {
  pc::unique_function<size_t(const char*)> f = std::strlen;

  EXPECT_TRUE(f);
  EXPECT_EQ(f("Hello"), 5u);
}

TEST(UniqueFunction, member_function_call) {
  pc::unique_function<int(const point&)> f = &point::dist_from_center;

  EXPECT_TRUE(f);
  EXPECT_EQ(f(point{3, 4}), 5);
}

TEST(UniqueFunction, member_object_get) {
  pc::unique_function<int(point)> f = &point::x;

  EXPECT_TRUE(f);
  EXPECT_EQ(f(point{100, 500}), 100);
}

TEST(UniqueFunction, functor_call) {
  pc::unique_function<std::string(const char*)> f = [accum = std::string{}](const char* val) mutable {
    return accum += val;
  };

  EXPECT_TRUE(f);
  EXPECT_EQ(f("Hello"), "Hello");
  EXPECT_EQ(f(" World"), "Hello World");
}

TEST(UniqueFunction, std_function_call) {
  std::function<std::string(const char*)> std_f = [accum = std::string{}](const char* val) mutable {
    return accum += val;
  };

  pc::unique_function<std::string(const char*)> f = std_f;

  EXPECT_TRUE(f);
  EXPECT_EQ(f("Hello"), "Hello");
  EXPECT_EQ(f(" World"), "Hello World");
  EXPECT_EQ(std_f("Hello"), "Hello");
}

TEST(UniqueFunction, refference_wrapper_call) {
  auto func = [accum = std::string{}](const char* val) mutable {
    return accum += val;
  };

  pc::unique_function<std::string(const char*)> f = std::ref(func);

  EXPECT_TRUE(f);
  EXPECT_EQ(f("Hello"), "Hello");
  EXPECT_EQ(func(" World"), "Hello World");
}

TEST(UniqueFunction, call_small_functor_after_move_ctor) {
  pc::unique_function<int(point)> f0 = &point::dist_from_center;
  EXPECT_TRUE(f0);
  pc::unique_function<int(point)> f1 = std::move(f0);
  EXPECT_TRUE(f1);
  EXPECT_FALSE(f0);
  EXPECT_EQ(f1(point{3, 4}), 5);
}

TEST(UniqueFunction, call_big_functor_after_move_ctor) {
  pc::unique_function<uint64_t(uint64_t)> f0 = [c = big{1, 2, 3, 4, 5, 6}](uint64_t x) {
    return x + c.u0 + c.u1 + c.u2 + c.u3 + c.u4 + c.u5;
  };
  EXPECT_TRUE(f0);
  pc::unique_function<uint64_t(uint64_t)> f1 = std::move(f0);
  EXPECT_TRUE(f1);
  EXPECT_FALSE(f0);
  EXPECT_EQ(f1(42), 63u);
}

TEST(UniqueFunction, call_small_functor_after_move_asign) {
  pc::unique_function<int(point)> f0 = &point::dist_from_center;
  pc::unique_function<int(point)> f1;

  EXPECT_TRUE(f0);
  EXPECT_FALSE(f1);

  f1 = std::move(f0);

  EXPECT_TRUE(f1);
  EXPECT_FALSE(f0);
  EXPECT_EQ(f1(point{3, 4}), 5);
}

TEST(UniqueFunction, call_big_functor_after_move_asign) {
  pc::unique_function<uint64_t(uint64_t)> f0 = [c = big{1, 2, 3, 4, 5, 6}](uint64_t x) {
    return x + c.u0 + c.u1 + c.u2 + c.u3 + c.u4 + c.u5;
  };
  pc::unique_function<uint64_t(uint64_t)> f1;
  EXPECT_TRUE(f0);
  EXPECT_FALSE(f1);

  f1 = std::move(f0);

  EXPECT_FALSE(f0);
  EXPECT_TRUE(f1);
  EXPECT_EQ(f1(42), 63u);
}

} // namespace test

} // anonymous namespace
