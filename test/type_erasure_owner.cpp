#include <memory>

#include <gtest/gtest.h>

#include <portable_concurrency/bits/alias_namespace.h>
#include <portable_concurrency/bits/type_erasure_owner.h>

namespace {

template<typename T>
struct adapter final: pc::detail::move_constructable {
  adapter(T val): val(std::move(val)) {}

  move_constructable* move_to(void* location, size_t space) noexcept final {
    void* obj_start = pc::detail::align(alignof(adapter), sizeof(adapter), location, space);
    assert(obj_start);
    return new(obj_start) adapter{std::move(val)};
  }

  T val;
};

using type_erasure_owner = pc::detail::type_erasure_owner<sizeof(adapter<uint64_t>), alignof(adapter<uint64_t>)>;

template<typename T>
using emplace_t = pc::detail::emplace_t<adapter<std::decay_t<T>>>;

TEST(TypeErasureOwner, default_constructed_is_empty) {
  type_erasure_owner empty;
  EXPECT_EQ(empty.get(), nullptr);
}

TEST(TypeErasureOwner, small_type_is_embeded) {
  type_erasure_owner var{emplace_t<uint16_t>{}, uint16_t{1632}};
  EXPECT_TRUE(var.is_embeded());
}

TEST(TypeErasureOwner, stored_small_object_has_correct_type) {
  type_erasure_owner var{emplace_t<uint16_t>{}, uint16_t{3259}};
  EXPECT_NE(dynamic_cast<adapter<uint16_t>*>(var.get()), nullptr);
}

TEST(TypeErasureOwner, stored_small_object_has_correct_value) {
  type_erasure_owner var{emplace_t<uint16_t>{}, uint16_t{47296}};
  EXPECT_EQ(dynamic_cast<adapter<uint16_t>*>(var.get())->val, 47296);
}

TEST(TypeErasureOwner, instance_with_embeded_objet_is_empty_after_move) {
  type_erasure_owner src{emplace_t<uint16_t>{}, uint16_t{47296}};
  type_erasure_owner dst = std::move(src);
  EXPECT_EQ(src.get(), nullptr);
}

TEST(TypeErasureOwner, small_object_remains_embeded_after_move) {
  type_erasure_owner src{emplace_t<uint16_t>{}, uint16_t{47296}};
  type_erasure_owner dst = std::move(src);
  EXPECT_TRUE(dst.is_embeded());
}

struct bigpt {uint64_t x; uint64_t y;};

TEST(TypeErasureOwner, big_type_is_heap_allocated) {
  type_erasure_owner var{emplace_t<bigpt>{}, bigpt{123, 456}};
  EXPECT_FALSE(var.is_embeded());
}

TEST(TypeErasureOwner, stored_big_object_has_correct_type) {
  type_erasure_owner var{emplace_t<bigpt>{}, bigpt{321, 654}};
  EXPECT_NE(dynamic_cast<adapter<bigpt>*>(var.get()), nullptr);
}

TEST(TypeErasureOwner, stored_big_object_has_correct_value) {
  type_erasure_owner var{emplace_t<bigpt>{}, bigpt{987, 543}};
  const bigpt& stored = dynamic_cast<adapter<bigpt>*>(var.get())->val;
  EXPECT_EQ(stored.x, 987);
  EXPECT_EQ(stored.y, 543);
}

TEST(TypeErasureOwner, instance_with_big_objet_is_empty_after_move) {
  type_erasure_owner src{emplace_t<bigpt>{}, bigpt{123, 100500}};
  type_erasure_owner dst = std::move(src);
  EXPECT_EQ(src.get(), nullptr);
}

TEST(TypeErasureOwner, big_object_remains_heap_allocated_after_move) {
  type_erasure_owner src{emplace_t<bigpt>{}, bigpt{123, 100500}};
  type_erasure_owner dst = std::move(src);
  EXPECT_FALSE(dst.is_embeded());
}

TEST(TypeErasureOwner, big_object_is_not_reallocated_by_move) {
  type_erasure_owner src{emplace_t<bigpt>{}, bigpt{123, 100500}};
  const auto* src_addr = src.get();
  type_erasure_owner dst = std::move(src);
  EXPECT_EQ(src_addr, dst.get());
}

struct move_only_int {
  move_only_int(int32_t val = 0): val(val) {}

  move_only_int(const move_only_int&) = delete;
  move_only_int(move_only_int&&) = default;

  move_only_int& operator= (move_only_int&&) = default;

  int32_t val;
};

TEST(TypeErasureOwner, non_copyable_small_type_is_embeded) {
  type_erasure_owner var{emplace_t<move_only_int>{}, 100500};
  EXPECT_TRUE(var.is_embeded());
}

TEST(TypeErasureOwner, move_instance_with_embeded_object_moves_content) {
  type_erasure_owner src{emplace_t<move_only_int>{}, 42};

  type_erasure_owner dest = std::move(src);
  EXPECT_EQ(dynamic_cast<adapter<move_only_int>*>(dest.get())->val.val, 42);
}

TEST(TypeErasureOwner, move_instance_with_heap_allocated_object_moves_content) {
  type_erasure_owner src{emplace_t<bigpt>{}, bigpt{32, 45}};

  type_erasure_owner dest = std::move(src);
  const bigpt& stored = dynamic_cast<adapter<bigpt>*>(dest.get())->val;
  EXPECT_EQ(stored.x, 32);
  EXPECT_EQ(stored.y, 45);
}

}
