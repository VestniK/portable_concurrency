#include <gtest/gtest.h>

#include <portable_concurrency/future>
#include <portable_concurrency/latch>
#include <portable_concurrency/timed_waiter>

#include "test_tools.h"

namespace portable_concurrency {
namespace {
namespace test {

using namespace std::literals;

template <typename Future> struct timed_waiter_poling : ::testing::Test {
  Future future;
  pc::timed_waiter waiter;

  timed_waiter_poling() {
    future = pc::async(g_future_tests_env, [] {
      std::this_thread::sleep_for(25ms);
      return 100500;
    });
    waiter = pc::timed_waiter{future};
  }
};
using poling_futures =
    ::testing::Types<pc::future<int>, pc::shared_future<int>>;
TYPED_TEST_CASE(timed_waiter_poling, poling_futures);

TYPED_TEST(timed_waiter_poling, future_is_ready_after_waiter_returns_ready) {
  while (this->waiter.wait_for(500us) == pc::future_status::timeout)
    ;
  EXPECT_TRUE(this->future.is_ready());
}

template <typename Future> struct timed_waiter : ::testing::Test {
  pc::latch task_latch;
  Future future;
  pc::timed_waiter waiter;

  timed_waiter() : task_latch{2} {
    future = pc::async(g_future_tests_env,
                       [this] { task_latch.count_down_and_wait(); });
    waiter = pc::timed_waiter{future};
  }
};
using futures = ::testing::Types<pc::future<void>, pc::shared_future<void>>;
TYPED_TEST_CASE(timed_waiter, futures);

TYPED_TEST(timed_waiter, wait_for_returns_when_future_becomes_ready) {
  this->task_latch.count_down();
  EXPECT_EQ(this->waiter.wait_for(30min), pc::future_status::ready);
}

TYPED_TEST(timed_waiter, wait_until_returns_when_future_becomes_ready) {
  this->task_latch.count_down();
  EXPECT_EQ(this->waiter.wait_until(std::chrono::system_clock::now() + 30min),
            pc::future_status::ready);
}

TYPED_TEST(timed_waiter, wait_for_returns_with_timeout_if_future_not_ready) {
  EXPECT_EQ(this->waiter.wait_for(5ms), pc::future_status::timeout);
  this->task_latch.count_down();
}

TYPED_TEST(timed_waiter, wait_until_returns_with_timeout_if_future_not_ready) {
  EXPECT_EQ(this->waiter.wait_until(std::chrono::steady_clock::now() + 5ms),
            pc::future_status::timeout);
  this->task_latch.count_down();
}

template <typename Future> struct timed_waiter_on_ready : ::testing::Test {
  Future future = pc::make_ready_future();
  pc::timed_waiter waiter;

  timed_waiter_on_ready() { waiter = pc::timed_waiter{future}; }
};
TYPED_TEST_CASE(timed_waiter_on_ready, futures);

TYPED_TEST(timed_waiter_on_ready, wait_until_returns_ready) {
  EXPECT_EQ(this->waiter.wait_until(std::chrono::steady_clock::now() + 30min),
            pc::future_status::ready);
}
TYPED_TEST(timed_waiter_on_ready, wait_for_returns_ready) {
  EXPECT_EQ(this->waiter.wait_for(2h), pc::future_status::ready);
}

} // namespace test
} // anonymous namespace
} // namespace portable_concurrency
