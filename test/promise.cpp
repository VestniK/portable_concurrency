#include <array>
#include <string>
#include <memory>

#include <gtest/gtest.h>

#include <portable_concurrency/future>

#include "test_helpers.h"
#include "test_tools.h"

namespace {

template<typename T, typename Arena>
struct arena_allocator {
  Arena& arena;

  using value_type = T;

  arena_allocator(Arena& arena):
    arena(arena)
  {}

  template<typename U>
  arena_allocator(const arena_allocator<U, Arena>& other):
    arena(other.arena)
  {}

  T* allocate(std::size_t count) {
    return reinterpret_cast<T*>(arena.allocate(sizeof(T) * count, alignof(T)));
  }

  void deallocate(void* ptr, std::size_t size) {}

  template<typename U>
  struct rebind {
    using other = arena_allocator<U, Arena>;
  };
};

template<typename Arena>
struct arena_allocator<void, Arena> {
  Arena& arena;

  using value_type = void;

  arena_allocator(Arena& arena):
    arena(arena)
  {}

  template<typename U>
  arena_allocator(const arena_allocator<U, Arena>& other):
    arena(other.arena)
  {}

  template<typename U>
  struct rebind {
    using other = arena_allocator<U, Arena>;
  };
};

template<typename T, typename Arena>
bool operator== (const arena_allocator<T, Arena>& lhs, const arena_allocator<T, Arena>& rhs) {
  return &lhs.arena == &rhs.arena;
}

template<size_t Size>
class static_arena {
  std::array<uint8_t, Size> data_;
  std::size_t offset_ = 0;
public:
  static_arena() = default;

  void* allocate(std::size_t bytes, std::size_t alignment) {
    void* start = data_.data() + offset_;
    size_t sz = data_.size() - offset_;
    void* result = std::align(alignment, bytes, start, sz);
    if (!result)
      throw std::bad_alloc{};
    offset_ = static_cast<size_t>(reinterpret_cast<std::uint8_t*>(result) - data_.data() + bytes);
    return result;
  }

  std::size_t used() const {
    return data_.size() - offset_;
  }
};

template<typename T>
struct PromiseTest: ::testing::Test {};
TYPED_TEST_CASE(PromiseTest, TestTypes);

namespace tests {

template<typename T>
void get_future_twice() {
  pc::promise<T> p;
  pc::future<T> f1, f2;
  EXPECT_NO_THROW(f1 = p.get_future());
  EXPECT_FUTURE_ERROR(
    f2 = p.get_future(),
    std::future_errc::future_already_retrieved
  );
}

template<typename T>
void set_val_on_promise_without_future() {
  pc::promise<T> p;
  EXPECT_NO_THROW(p.set_value(some_value<T>()));
}

template<>
void set_val_on_promise_without_future<void>(){
  pc::promise<void> p;
  EXPECT_NO_THROW(p.set_value());
}

template<typename T>
void set_err_on_promise_without_future() {
  pc::promise<T> p;
  EXPECT_NO_THROW(p.set_exception(std::make_exception_ptr(std::runtime_error("error"))));
}

template<typename T>
void set_value_twice_without_future() {
  pc::promise<T> p;
  EXPECT_NO_THROW(p.set_value(some_value<T>()));
  EXPECT_FUTURE_ERROR(p.set_value(some_value<T>()), std::future_errc::promise_already_satisfied);
}

template<>
void set_value_twice_without_future<void>() {
  pc::promise<void> p;
  EXPECT_NO_THROW(p.set_value());
  EXPECT_FUTURE_ERROR(p.set_value(), std::future_errc::promise_already_satisfied);
}

template<typename T>
void set_value_twice_with_future() {
  pc::promise<T> p;
  auto f = p.get_future();

  EXPECT_NO_THROW(p.set_value(some_value<T>()));
  EXPECT_TRUE(f.valid());
  EXPECT_TRUE(f.is_ready());

  EXPECT_FUTURE_ERROR(p.set_value(some_value<T>()), std::future_errc::promise_already_satisfied);
  EXPECT_TRUE(f.valid());
  EXPECT_TRUE(f.is_ready());
}

template<>
void set_value_twice_with_future<void>() {
  pc::promise<void> p;
  auto f = p.get_future();

  EXPECT_NO_THROW(p.set_value());
  EXPECT_TRUE(f.valid());
  EXPECT_TRUE(f.is_ready());

  EXPECT_FUTURE_ERROR(p.set_value(), std::future_errc::promise_already_satisfied);
  EXPECT_TRUE(f.valid());
  EXPECT_TRUE(f.is_ready());
}

template<typename T>
void set_value_twice_with_future_using_allocator() {
  static_arena<4096> arena;
  arena_allocator<std::decay_t<T>, static_arena<4096>> alloc(arena);

  pc::promise<T> p(std::allocator_arg, alloc);
  auto f = p.get_future();

  EXPECT_NO_THROW(p.set_value(some_value<T>()));
  EXPECT_TRUE(f.valid());
  EXPECT_TRUE(f.is_ready());

  EXPECT_FUTURE_ERROR(p.set_value(some_value<T>()), std::future_errc::promise_already_satisfied);
  EXPECT_TRUE(f.valid());
  EXPECT_TRUE(f.is_ready());

  EXPECT_GT(arena.used(), 0);
}

template<>
void set_value_twice_with_future_using_allocator<void>() {
  static_arena<4096> arena;
  arena_allocator<void, static_arena<4096>> alloc(arena);

  pc::promise<void> p(std::allocator_arg, alloc);
  auto f = p.get_future();

  EXPECT_NO_THROW(p.set_value());
  EXPECT_TRUE(f.valid());
  EXPECT_TRUE(f.is_ready());

  EXPECT_FUTURE_ERROR(p.set_value(), std::future_errc::promise_already_satisfied);
  EXPECT_TRUE(f.valid());
  EXPECT_TRUE(f.is_ready());

  EXPECT_GT(arena.used(), 0);
}

template<typename T>
void set_value_twice_after_value_taken() {
  pc::promise<T> p;
  auto f = p.get_future();

  EXPECT_NO_THROW(p.set_value(some_value<T>()));
  EXPECT_TRUE(f.valid());
  EXPECT_TRUE(f.is_ready());
  f.get();
  EXPECT_FALSE(f.valid());

  EXPECT_FUTURE_ERROR(p.set_value(some_value<T>()), std::future_errc::promise_already_satisfied);
  EXPECT_FALSE(f.valid());
}

template<>
void set_value_twice_after_value_taken<void>() {
  pc::promise<void> p;
  auto f = p.get_future();

  EXPECT_NO_THROW(p.set_value());
  EXPECT_TRUE(f.valid());
  EXPECT_TRUE(f.is_ready());
  f.get();
  EXPECT_FALSE(f.valid());

  EXPECT_FUTURE_ERROR(p.set_value(), std::future_errc::promise_already_satisfied);
  EXPECT_FALSE(f.valid());
}

} // namespace tests

TYPED_TEST(PromiseTest, get_future_twice) {tests::get_future_twice<TypeParam>();}
TYPED_TEST(PromiseTest, set_val_on_promise_without_future) {tests::set_val_on_promise_without_future<TypeParam>();}
TYPED_TEST(PromiseTest, set_err_on_promise_without_future) {tests::set_err_on_promise_without_future<TypeParam>();}
TYPED_TEST(PromiseTest, set_value_twice_without_future) {tests::set_value_twice_without_future<TypeParam>();}
TYPED_TEST(PromiseTest, set_value_twice_with_future) {tests::set_value_twice_with_future<TypeParam>();}
TYPED_TEST(PromiseTest, set_value_twice_with_future_using_allocator) {tests::set_value_twice_with_future_using_allocator<TypeParam>();}
// Current cancellation implementation looses information if value was set after last future or shared future is
// destroyed or becomes invalid. Disabling this test until cancellation implementation improvement.
TYPED_TEST(PromiseTest, DISABLED_set_value_twice_after_value_taken) {
  tests::set_value_twice_after_value_taken<TypeParam>();
}

TYPED_TEST(PromiseTest, can_retreive_value_set_before_get_future) {
  pc::promise<TypeParam> promise;
  set_promise_value(promise);
  pc::future<TypeParam> future = promise.get_future();
  EXPECT_SOME_VALUE(future);
}

TYPED_TEST(PromiseTest, abandon_promise_set_proper_error) {
  pc::future<TypeParam> future;
  {
    pc::promise<TypeParam> promise;
    future = promise.get_future();
  }
  EXPECT_FUTURE_ERROR(future.get(), std::future_errc::broken_promise);
}

TYPED_TEST(PromiseTest, moved_from_throws_no_state_on_get_future) {
  pc::promise<TypeParam> promise;
  pc::promise<TypeParam> another_promise{std::move(promise)};
  EXPECT_FUTURE_ERROR(promise.get_future(), std::future_errc::no_state);
}

TYPED_TEST(PromiseTest, moved_from_throws_no_state_on_set_exception) {
  pc::promise<TypeParam> promise;
  pc::promise<TypeParam> another_promise{std::move(promise)};
  EXPECT_FUTURE_ERROR(
    promise.set_exception(std::make_exception_ptr(std::runtime_error{"Ooups"})),
    std::future_errc::no_state
  );
}

TYPED_TEST(PromiseTest, moved_from_throws_no_state_on_set_value) {
  pc::promise<TypeParam> promise;
  pc::promise<TypeParam> another_promise{std::move(promise)};
  EXPECT_FUTURE_ERROR(set_promise_value(promise), std::future_errc::no_state);
}

TEST(Promise, is_awaiten_returns_true_before_get_fututre_call) {
  pc::promise<void> p;
  EXPECT_TRUE(p.is_awaiten());
}

} // anonymous namespace
