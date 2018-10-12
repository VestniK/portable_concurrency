#include <portable_concurrency/latch>

#include "test_tools.h"

future_tests_env* g_future_tests_env = static_cast<future_tests_env*>(
  ::testing::AddGlobalTestEnvironment(new future_tests_env{std::max(3u, std::thread::hardware_concurrency())})
);

null_executor_t null_executor;

future_tests_env::future_tests_env(size_t threads_num):
  pool_{threads_num}
{
  tids_.reserve(threads_num);
  pc::latch latch{static_cast<ptrdiff_t>(threads_num)};
  std::mutex mtx;
  while (threads_num --> 0) {
    post(pool_.executor(), [this, &latch, &mtx] {
      std::lock_guard<std::mutex>{mtx}, tids_.push_back(std::this_thread::get_id());
      latch.count_down_and_wait();
    });
  }
  latch.wait();
}

void future_tests_env::TearDown() {
  pool_.wait();
}

bool future_tests_env::uses_thread(std::thread::id id) const {
  return std::find(tids_.begin(), tids_.end(), id) != tids_.end();
}

void future_tests_env::wait_current_tasks() {
  if (uses_thread(std::this_thread::get_id())) {
    throw std::system_error{
      std::make_error_code(std::errc::resource_deadlock_would_occur),
      "wait_current_tasks from worker thread"
    };
  }

  pc::latch latch{static_cast<ptrdiff_t>(tids_.size())};
  for (size_t i = 0; i < tids_.size(); ++i)
    post(pool_.executor(), [&latch] {latch.count_down_and_wait();});
  latch.wait();
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
