#include "test_tools.h"

future_tests_env* g_future_tests_env = static_cast<future_tests_env*>(
  ::testing::AddGlobalTestEnvironment(new future_tests_env)
);

void future_tests_env::SetUp() {
  for (auto& worker: workers_) {
    worker = std::thread([](closable_queue<task>& queue) {
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
    }, std::ref(queue_));
  }
}

void future_tests_env::TearDown() {
  queue_.close();
  for (auto& worker: workers_) {
    if (worker.joinable())
        worker.join();
  }
}
