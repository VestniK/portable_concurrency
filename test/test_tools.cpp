#include "closable_queue.hpp"
#include "test_tools.h"

future_tests_env* g_future_tests_env = static_cast<future_tests_env*>(
  ::testing::AddGlobalTestEnvironment(new future_tests_env)
);

null_executor_t null_executor;

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

template class closable_queue<pc::unique_function<void()>>;

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

bool future_tests_env::uses_thread(std::thread::id id) const {
  return std::find_if(
    workers_.begin(), workers_.end(), [id](const std::thread& t) {return t.get_id() == id;}
  ) != workers_.end();
}

void future_tests_env::wait_current_tasks() {
  if (uses_thread(std::this_thread::get_id())) {
    throw std::system_error{
      std::make_error_code(std::errc::resource_deadlock_would_occur),
      "wait_current_tasks from worker thread"
    };
  }

  queue_.wait_empty();
}

future_test::~future_test() {g_future_tests_env->wait_current_tasks();}

std::ostream& operator<< (std::ostream& out, const printable<std::unique_ptr<int>>& printable) {
  return out << *(printable.value);
}

std::ostream& operator<< (std::ostream& out, const printable<future_tests_env&>& printable) {
  return out << "future_tests_env instance: 0x" << std::hex << reinterpret_cast<std::uintptr_t>(&printable.value);
}

void expect_future_exception(pc::future<void>& future, const std::string& what) {
  try {
    future.get();
    ADD_FAILURE() << "void value was returned instead of exception";
  } catch(const std::runtime_error& err) {
    EXPECT_EQ(what, err.what());
  } catch(const std::exception& err) {
    ADD_FAILURE() << "Unexpected exception: " << err.what();
  } catch(...) {
    ADD_FAILURE() << "Unexpected unknown exception type";
  }
}

void expect_future_exception(const pc::shared_future<void>& future, const std::string& what) {
  try {
    future.get();
    ADD_FAILURE() << "void value was returned instead of exception";
  } catch(const std::runtime_error& err) {
    EXPECT_EQ(what, err.what());
  } catch(const std::exception& err) {
    ADD_FAILURE() << "Unexpected exception: " << err.what();
  } catch(...) {
    ADD_FAILURE() << "Unexpected unknown exception type";
  }
}
