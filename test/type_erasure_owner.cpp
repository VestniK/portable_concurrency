#include <string>
#include <sstream>

#include <gtest/gtest.h>

#include "../portable_concurrency/bits/type_erasure_owner.h"

namespace {

struct stringifiable {
  virtual ~stringifiable() = default;
  virtual stringifiable* move_to(char*) && noexcept = 0;
  virtual std::string to_string() const = 0;
};

template<typename T>
struct stringifiable_adapter:
  public portable_concurrency::detail::move_erased<stringifiable, stringifiable_adapter<T>>
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
  public portable_concurrency::detail::move_erased<stringifiable, stringifiable_adapter<std::string>>
{
public:
  stringifiable_adapter(std::string val): val_(std::move(val)) {}

  std::string to_string() const override {
    return val_;
  }

private:
  std::string val_;
};

using type_erasure_owner = portable_concurrency::detail::type_erasure_owner_t<
  stringifiable,
  stringifiable_adapter,
  std::string, const char*
>;

TEST(TypeErasureOwner, empty) {
  type_erasure_owner empty;
  EXPECT_EQ(empty.get(), nullptr);
}

TEST(TypeErasureOwner, object_is_embeded) {
  type_erasure_owner var{portable_concurrency::detail::emplace_t<stringifiable_adapter<std::string>>{}, "Hello"};
  EXPECT_NE(var.get(), nullptr);
  EXPECT_TRUE(var.is_embeded());
  EXPECT_EQ(var.get()->to_string(), "Hello");
}

}
