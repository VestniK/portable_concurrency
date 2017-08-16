#include "test_tools.h"

future_tests_env* g_future_tests_env = static_cast<future_tests_env*>(
  ::testing::AddGlobalTestEnvironment(new future_tests_env)
);

namespace {

void worker_function(closable_queue<pc::unique_function<void()>>& queue) {
  while (true) try {
    auto t = queue.pop();
    t();
  } catch (const queue_closed&) {
    break;
  } catch (const std::exception& e) {
    ADD_FAILURE() << "Uncaught exception in worker thread: " << e.what();
  } catch (...) {
    ADD_FAILURE() << "Uncaught exception of unknown type in worker thread";
  }
}

} // anonymous namespace

void future_tests_env::SetUp() {
  const auto threads_count = std::max(3u, std::thread::hardware_concurrency());
  for (workers_.reserve(threads_count); workers_.size() < threads_count;)
    workers_.emplace_back(worker_function, std::ref(queue_));
}

void future_tests_env::TearDown() {
  queue_.close();
  for (auto& worker: workers_) {
    if (worker.joinable())
      worker.join();
  }
}
