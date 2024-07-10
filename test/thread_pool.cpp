#include <gtest/gtest.h>

#include <portable_concurrency/latch>
#include <portable_concurrency/thread_pool>

#include <atomic>

namespace {

TEST(ThreadPool, should_execute_function_in_another_thread) {
  std::thread::id tid;
  std::mutex mtx;
  std::condition_variable cv;

  pc::static_thread_pool pool{1};
  post(pool.executor(), [&] {
    {
      std::lock_guard<std::mutex> lk{mtx};
      tid = std::this_thread::get_id();
    }
    cv.notify_one();
  });
  std::unique_lock<std::mutex> lock{mtx};
  cv.wait(lock, [&] { return tid != std::thread::id{}; });
  EXPECT_NE(tid, std::this_thread::get_id());
}

TEST(ThreadPool, processes_all_queued_tasks_when_waited_on) {
  static constexpr size_t task_count = 1;

  pc::static_thread_pool pool{1};

  std::atomic<size_t> processed_tasks_count{0};
  pc::latch latch{2};

  post(pool.executor(), [&latch] {
    latch.count_down_and_wait();
  });

  for (size_t i = 0; i < task_count; ++i) {
    post(pool.executor(), [&processed_tasks_count] {
      ++processed_tasks_count;
    });
  }

  latch.count_down_and_wait();
  pool.wait();

  EXPECT_EQ(processed_tasks_count.load(), task_count);
}

TEST(ThreadPool, abandons_unprocessed_tasks_when_stopped) {
  static constexpr size_t task_count = 1;

  pc::static_thread_pool pool{1};

  std::atomic<size_t> processed_tasks_count{0};
  pc::latch latch_before_stop{2};
  pc::latch latch_after_stop{2};

  post(pool.executor(), [&latch_before_stop, &latch_after_stop] {
    latch_before_stop.count_down_and_wait();
    latch_after_stop.count_down_and_wait();
  });

  for (size_t i = 0; i < task_count; ++i) {
    post(pool.executor(), [&processed_tasks_count] {
      ++processed_tasks_count;
    });
  }

  latch_before_stop.count_down_and_wait();
  pool.stop();
  latch_after_stop.count_down_and_wait();
  pool.wait();

  EXPECT_EQ(processed_tasks_count.load(), 0);
}

} // namespace
