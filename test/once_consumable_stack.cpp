#include <chrono>
#include <map>
#include <thread>
#include <algorithm>
#include <vector>

#include <gtest/gtest.h>

#include <portable_concurrency/future>
#include <portable_concurrency/latch>

#include <portable_concurrency/bits/once_consumable_stack.hpp>

#include "test_tools.h"

using namespace std::literals;

template <typename T>
using stack = pc::detail::once_consumable_stack<T>;

namespace {

struct record {
  size_t id;
  size_t task_id;
};

record producer(stack<record>& records_stack, pc::latch& latch, size_t task_id) {
  latch.count_down_and_wait();
  record rec = {0, task_id};
  for (; rec.id < 1'000'000; ++rec.id) {
    auto rec_to_push = rec;
    if (!records_stack.push(rec_to_push))
      break;
  }
  return rec;
}

template <typename It>
::testing::AssertionResult monotonic_sequence(const char*, const char*, It first, It last) {
  const auto it = std::adjacent_find(first, last, [](record lhs, record rhs) { return rhs.id - lhs.id != 1; });
  if (it == last)
    return ::testing::AssertionSuccess();

  if (it->id == (it + 1)->id)
    return ::testing::AssertionFailure() << "Dublicated record {task_id: " << it->task_id << ", id: " << it->id << "}";

  return ::testing::AssertionFailure() << "Task " << it->task_id << " has missing records between " << it->id << " and "
                                       << (it + 1)->id;
}

} // namespace

TEST(OnceConsumableQueueTests, concurrent_push_until_consume) {
  stack<record> records_stack;
  pc::latch latch{static_cast<ptrdiff_t>(g_future_tests_env->threads_count() + 1)};

  std::vector<pc::future<record>> futures;
  futures.reserve(g_future_tests_env->threads_count());
  for (size_t i = 0; i < g_future_tests_env->threads_count(); ++i) {
    auto task = pc::packaged_task<record(stack<record>&, pc::latch&, size_t)>{producer};
    futures.push_back(task.get_future());
    g_future_tests_env->run_async(std::move(task), std::ref(records_stack), std::ref(latch), i);
  }

  latch.count_down_and_wait();
  std::this_thread::sleep_for(25ms);

  std::deque<record> records;
  for (const auto& rec : records_stack.consume())
    records.push_back(rec);

  std::sort(records.begin(), records.end(),
      [](record lhs, record rhs) { return std::tie(lhs.task_id, lhs.id) < std::tie(rhs.task_id, rhs.id); });

  for (auto& f : futures) {
    const record last = f.get();
    auto ids_range = std::equal_range(
        records.begin(), records.end(), last, [](record lhs, record rhs) { return lhs.task_id < rhs.task_id; });
    if (ids_range.first == ids_range.second) {
      EXPECT_EQ(last.id, 0u);
      continue;
    }
    EXPECT_EQ((ids_range.second - 1)->id + 1, last.id);
    EXPECT_PRED_FORMAT2(monotonic_sequence, ids_range.first, ids_range.second);
  }
}
