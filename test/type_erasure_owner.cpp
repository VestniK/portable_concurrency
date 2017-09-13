#include <cstring>
#include <memory>
#include <sstream>
#include <string>
#include <system_error>

#include <gtest/gtest.h>

#include "../portable_concurrency/bits/type_erasure_owner.h"

namespace {

namespace pc = portable_concurrency;

struct stringifiable {
  virtual ~stringifiable() = default;
  virtual stringifiable* move_to(char*) && noexcept = 0;
  virtual std::string to_string() const = 0;
};

template<typename T>
struct stringifiable_adapter:
  public pc::detail::move_erased<stringifiable, stringifiable_adapter<T>>
{
public:
  stringifiable_adapter(T val): val_(std::move(val)) {}

  std::string to_string() const override {
    std::ostringstream oss;
    oss << val_;
    return oss.str();
  }

private:
  T val_;
};

template<>
class stringifiable_adapter<std::string>:
  public pc::detail::move_erased<stringifiable, stringifiable_adapter<std::string>>
{
public:
  stringifiable_adapter(std::string val): val_(std::move(val)) {}

  std::string to_string() const override {
    return val_;
  }

private:
  std::string val_;
};

using type_erasure_owner = pc::detail::type_erasure_owner_t<
  stringifiable,
  stringifiable_adapter,
  std::string, const char*
>;

template<typename T>
using emplace_t = pc::detail::emplace_t<stringifiable_adapter<T>>;

TEST(TypeErasureOwner, empty) {
  type_erasure_owner empty;
  EXPECT_EQ(empty.get(), nullptr);
}

TEST(TypeErasureOwner, first_enlisted_type_is_embeded) {
  type_erasure_owner var{emplace_t<std::string>{}, "Hello"};
  EXPECT_NE(var.get(), nullptr);
  EXPECT_TRUE(var.is_embeded());
  EXPECT_EQ(var.get()->to_string(), "Hello");
}

TEST(TypeErasureOwner, second_enlisted_type_is_embeded) {
  type_erasure_owner var{emplace_t<const char*>{}, "Hello"};
  EXPECT_NE(var.get(), nullptr);
  EXPECT_TRUE(var.is_embeded());
  EXPECT_EQ(var.get()->to_string(), "Hello");
}

TEST(TypeErasureOwner, small_type_is_embeded) {
  static_assert(
    sizeof(double) < sizeof(std::string),
    "Test assumptions on types size are not satisfied"
  );
  static_assert(
    alignof(double) <= alignof(std::string),
    "Test assumptions on types alignment are not satisfied"
  );
  type_erasure_owner var{emplace_t<double>{}, 2.37};
  EXPECT_NE(var.get(), nullptr);
  EXPECT_TRUE(var.is_embeded());
  EXPECT_EQ(var.get()->to_string(), "2.37");
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
  EXPECT_EQ(var.get()->to_string(), "open: " + ec.message());
}

using unique_str = std::unique_ptr<char[]>;
template<>
class stringifiable_adapter<unique_str>:
  public pc::detail::move_erased<stringifiable, stringifiable_adapter<unique_str>>
{
public:
  stringifiable_adapter(const char* str):
    val_(new char[std::strlen(str)])
  {
    std::strcpy(val_.get(), str);
  }

  std::string to_string() const override {
    return val_.get();
  }

private:
  unique_str val_;
};

TEST(TypeErasureOwner, non_copyable_small_type) {
  type_erasure_owner var{emplace_t<unique_str>{}, "Unique str"};
  EXPECT_NE(var.get(), nullptr);
  EXPECT_TRUE(var.is_embeded());
  EXPECT_EQ(var.get()->to_string(), "Unique str");
}

}
