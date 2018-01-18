#include <cstring>
#include <memory>
#include <sstream>
#include <string>
#include <system_error>

#include <gtest/gtest.h>

#include <portable_concurrency/bits/alias_namespace.h>
#include <portable_concurrency/bits/type_erasure_owner.h>

namespace {

#if defined(_MSC_VER) || (defined(__GNUC__) && (__GNUC__ < 5 || defined(_GLIBCXX_DEBUG)))
template<template<typename> class Adapter, typename... T>
struct type_erasure_owner_t_helper {
#if defined(_MSC_VER) && (_MSC_VER > 1900)
  static constexpr size_t storage_size = std::max(std::initializer_list<size_t>{sizeof(Adapter<T>)...});
  static constexpr size_t storage_align = std::max(std::initializer_list<size_t>{alignof(Adapter<T>)...});
#else
  constexpr static size_t max_sz(size_t sz) {return sz;}

  template<typename... Size_t>
  constexpr static size_t max_sz(size_t sz, Size_t... szs) {
    return sz > max_sz(szs...) ? sz : max_sz(szs...);
  }

  static constexpr size_t storage_size = max_sz(sizeof(Adapter<T>)...);
  static constexpr size_t storage_align = max_sz(alignof(Adapter<T>)...);
#endif
  using type = pc::detail::type_erasure_owner<storage_size, storage_align>;
};

template<template<typename> class Adapter, typename... T>
using type_erasure_owner_t = typename type_erasure_owner_t_helper<Adapter, T...>::type;
#else
template<template<typename> class Adapter, typename... T>
using type_erasure_owner_t = pc::detail::type_erasure_owner<
  std::max({sizeof(Adapter<T>)...}),
  std::max({alignof(Adapter<T>)...})
>;
#endif

struct stringifiable: pc::detail::move_constructable {
  virtual std::string to_string() const = 0;
};

template<typename T>
struct stringifiable_adapter final: stringifiable {
  stringifiable_adapter(T val): val(std::move(val)) {}

  std::string to_string() const override {
    std::ostringstream oss;
    oss << val;
    return oss.str();
  }

  move_constructable* move_to(void* location, size_t space) noexcept final {
    void* obj_start = pc::detail::align(alignof(stringifiable_adapter), sizeof(stringifiable_adapter), location, space);
    assert(obj_start);
    return new(obj_start) stringifiable_adapter{std::move(val)};
  }

  T val;
};

using type_erasure_owner = type_erasure_owner_t<stringifiable_adapter, std::string, const char*>;
std::string to_string(const type_erasure_owner& val) {
  return static_cast<const stringifiable*>(val.get())->to_string();
}

template<typename T>
using emplace_t = pc::detail::emplace_t<stringifiable_adapter<std::decay_t<T>>>;

TEST(TypeErasureOwner, empty) {
  type_erasure_owner empty;
  EXPECT_EQ(empty.get(), nullptr);
}

TEST(TypeErasureOwner, first_enlisted_type_is_embeded) {
  type_erasure_owner var{emplace_t<std::string>{}, "Hello"};
  EXPECT_NE(var.get(), nullptr);
  EXPECT_TRUE(var.is_embeded());
  EXPECT_EQ(to_string(var), "Hello");
}

TEST(TypeErasureOwner, second_enlisted_type_is_embeded) {
  type_erasure_owner var{emplace_t<const char*>{}, "Hello"};
  EXPECT_NE(var.get(), nullptr);
  EXPECT_TRUE(var.is_embeded());
  EXPECT_EQ(to_string(var), "Hello");
}

TEST(TypeErasureOwner, small_type_is_embeded) {
  static_assert(
    sizeof(uint16_t) <= sizeof(std::string),
    "Test assumptions on types size are not satisfied"
  );
  static_assert(
    alignof(uint16_t) <= alignof(std::string),
    "Test assumptions on types alignment are not satisfied"
  );
  type_erasure_owner var{emplace_t<uint16_t>{}, uint16_t{1632}};
  EXPECT_NE(var.get(), nullptr);
  EXPECT_TRUE(var.is_embeded());
  EXPECT_EQ(to_string(var), "1632");
}

struct error {
  std::string scope;
  std::error_code ec;
};

std::ostream& operator<< (std::ostream& out, const error& val) {
  return out << val.scope << ": " << val.ec.message();
}

TEST(TypeErasureOwner, big_type_is_heap_allocated) {
  const auto ec = std::make_error_code(std::errc::permission_denied);
  type_erasure_owner var{emplace_t<error>{}, error{"open", ec}};
  EXPECT_NE(var.get(), nullptr);
  EXPECT_FALSE(var.is_embeded());
  EXPECT_EQ(to_string(var), "open: " + ec.message());
}

using unique_str = std::unique_ptr<char[]>;
std::ostream& operator<< (std::ostream& out, const unique_str& val) {
  return out << val.get();
}

unique_str make_unique_str(const char* str) {
  unique_str res(new char[std::strlen(str) + 1]);
  std::strcpy(res.get(), str);
  return res;
}

TEST(TypeErasureOwner, non_copyable_small_type) {
  type_erasure_owner var{emplace_t<unique_str>{}, make_unique_str("Unique str")};
  EXPECT_NE(var.get(), nullptr);
  EXPECT_TRUE(var.is_embeded());
  EXPECT_EQ(to_string(var), "Unique str");
}

TEST(TypeErasureOwner, move_first_enlisted_type) {
  type_erasure_owner src{emplace_t<std::string>{}, "Hello"};

  type_erasure_owner dest = std::move(src);
  EXPECT_NE(dest.get(), nullptr);
  EXPECT_EQ(src.get(), nullptr);
  EXPECT_TRUE(dest.is_embeded());
  EXPECT_EQ(to_string(dest), "Hello");
}

TEST(TypeErasureOwner, move_second_enlisted_type) {
  type_erasure_owner src{emplace_t<const char*>{}, "Hello C str"};

  type_erasure_owner dest = std::move(src);
  EXPECT_NE(dest.get(), nullptr);
  EXPECT_EQ(src.get(), nullptr);
  EXPECT_TRUE(dest.is_embeded());
  EXPECT_EQ(to_string(dest), "Hello C str");
}

TEST(TypeErasureOwner, move_small_type) {
  static_assert(
    sizeof(uint16_t) <= sizeof(std::string),
    "Test assumptions on types size are not satisfied"
  );
  static_assert(
    alignof(uint16_t) <= alignof(std::string),
    "Test assumptions on types alignment are not satisfied"
  );
  type_erasure_owner src{emplace_t<uint16_t>{}, uint16_t{324}};

  type_erasure_owner dest = std::move(src);
  EXPECT_NE(dest.get(), nullptr);
  EXPECT_EQ(src.get(), nullptr);
  EXPECT_TRUE(dest.is_embeded());
  EXPECT_EQ(to_string(dest), "324");
}

TEST(TypeErasureOwner, move_big_type) {
  const auto ec = std::make_error_code(std::errc::permission_denied);
  type_erasure_owner src{emplace_t<error>{}, error{"open", ec}};

  type_erasure_owner dest = std::move(src);
  EXPECT_NE(dest.get(), nullptr);
  EXPECT_EQ(src.get(), nullptr);
  EXPECT_FALSE(dest.is_embeded());
  EXPECT_EQ(to_string(dest), "open: " + ec.message());
}

}
