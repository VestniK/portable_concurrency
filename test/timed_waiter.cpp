#include <gtest/gtest.h>

#include <portable_concurrency/future>
#include <portable_concurrency/timed_waiter>

#include "test_tools.h"

namespace portable_concurrency {
namespace {
namespace test {

using namespace std::literals;

TEST(timed_waiter, can_be_used_for_poling) {
  pc::future<void> future = pc::async(g_future_tests_env, [] { std::this_thread::sleep_for(25ms); });
  pc::timed_waiter waiter{future};
  while (waiter.wait_for(500us) == pc::future_status::timeout)
    ;
  EXPECT_TRUE(future.is_ready());
}

} // namespace test
} // anonymous namespace
} // namespace portable_concurrency
