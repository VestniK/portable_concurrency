#include <gtest/gtest.h>

#include <portable_concurrency/thread_pool>

namespace {

TEST(ThreadPool, should_execute_function_in_another_thread) {
  std::thread::id tid;
  std::mutex mtx;
  std::condition_variable cv;

  pc::static_thread_pool pool{1};
  post(pool.executor(), [&] {
    std::lock_guard<std::mutex>{mtx}, tid = std::this_thread::get_id();
    cv.notify_one();
  });
  std::unique_lock<std::mutex> lock{mtx};
  cv.wait(lock, [&] { return tid != std::thread::id{}; });
  EXPECT_NE(tid, std::this_thread::get_id());
}

} // namespace
