#include <gtest/gtest.h>

#include <portable_concurrency/future>

TEST(packaged_task_unwrap, retruns_future_of_proper_type) {
  pc::packaged_task<pc::future<int>()> task{[] { return pc::future<int>{}; }};
  auto future = task.get_future();
  static_assert(std::is_same<decltype(future), pc::future<int>>::value, "invalid future type");
}
