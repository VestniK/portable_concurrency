#include <chrono>
#include <map>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include "concurrency/future"
#include "concurrency/latch"

#include "concurrency/bits/once_consumable_queue.h"

#include "test_tools.h"
using namespace std::literals;

template<typename T>
using queue = experimental::detail::once_consumable_queue<T>;

TEST(OnceConsumableQueueTests, concurrent_push_until_consume) {
  struct record {
    size_t id = 0;
    std::thread::id tid = std::this_thread::get_id();
  };
  queue<record> records_queue;
  experimental::latch latch{
    static_cast<ptrdiff_t>(g_future_tests_env->threads_count() + 1)
  };
  auto producer = [&records_queue, &latch]() {
    latch.count_down_and_wait();
    record rec = {};
    for (;rec.id < 1'000'000; ++rec.id) {
      auto rec_to_push = rec;
      if (!records_queue.push(rec_to_push))
        break;
    }
    return rec;
  };

  std::vector<experimental::future<record>> futures;
  futures.reserve(g_future_tests_env->threads_count());
  for (size_t i = 0; i < g_future_tests_env->threads_count(); ++i) {
    auto task = experimental::packaged_task<record()>{producer};
    futures.push_back(task.get_future());
    g_future_tests_env->run_async(std::move(task));
  }

  latch.count_down_and_wait();
  std::this_thread::sleep_for(50ms);
  std::map<std::thread::id, size_t> max_thread_rec_id;
  for (const auto& rec: records_queue.consume()) {
    auto it = max_thread_rec_id.find(rec.tid);
    if (it == max_thread_rec_id.end())
      max_thread_rec_id.emplace(rec.tid, rec.id);
    else
      it->second = std::max(it->second, rec.id);
  }

  for (auto& rec_f: futures) {
    auto rec = rec_f.get();
    ASSERT_EQ(max_thread_rec_id.count(rec.tid), 1u);
    EXPECT_EQ(max_thread_rec_id[rec.tid] + 1, rec.id);
  }
}
