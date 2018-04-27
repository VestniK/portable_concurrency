#pragma once

#include <iostream>
#include <stdexcept>
#include <thread>

#include <gtest/gtest.h>

#include <portable_concurrency/functional>
#include <portable_concurrency/future>

#include "closable_queue.h"
#include "task.h"

#define EXPECT_FUTURE_ERROR(statement, errc) \
  try { \
    statement; \
    ADD_FAILURE() << "Expected exception was not thrown"; \
  } catch(const std::future_error& err) { \
    EXPECT_EQ(err.code(), errc) << "received error message: " << err.code().message(); \
  } catch(const std::exception& err) { \
    ADD_FAILURE() << "Unexpected exception: " << err.what(); \
  } catch(...) { \
    ADD_FAILURE() << "Unexpected unknown exception type"; \
  }

extern template class closable_queue<pc::unique_function<void()>>;

class future_tests_env: public ::testing::Environment {
public:
  void SetUp() override;

  void TearDown() override;

  template<typename F, typename... A>
  void run_async(F&& f, A&&... a) {
    queue_.push(make_task(std::forward<F>(f), std::forward<A>(a)...));
  }

  size_t threads_count() const {return workers_.size();}
  bool uses_thread(std::thread::id id) const;
  void wait_current_tasks();

private:
  std::vector<std::thread> workers_;
  closable_queue<pc::unique_function<void()>> queue_;
};

namespace portable_concurrency {
template<> struct is_executor<future_tests_env*>: std::true_type {};
}

template<typename F, typename... A>
void post(future_tests_env* exec, F&& f, A&&... a) {
  exec->run_async(std::forward<F>(f), std::forward<A>(a)...);
}

extern
future_tests_env* g_future_tests_env;

struct future_test: ::testing::Test {
  ~future_test();
};

struct null_executor_t {};
template<typename F>
void post(null_executor_t, F&&) {}

namespace portable_concurrency {
template<> struct is_executor<null_executor_t>: std::true_type {};
}
extern null_executor_t null_executor;

template<typename T>
struct printable {
  const T& value;
};

template<typename T>
std::ostream& operator<< (std::ostream& out, const printable<T>& printable) {
  return out << printable.value;
}

std::ostream& operator<< (std::ostream& out, const printable<std::unique_ptr<int>>& printable);
std::ostream& operator<< (std::ostream& out, const printable<future_tests_env&>& printable);

template<typename T>
void expect_future_exception(pc::future<T>& future, const std::string& what) {
  try {
    T unexpected_res = future.get();
    ADD_FAILURE() << "Value " << printable<T>{unexpected_res} << " was returned instead of exception";
  } catch(const std::runtime_error& err) {
    EXPECT_EQ(what, err.what());
  } catch(const std::exception& err) {
    ADD_FAILURE() << "Unexpected exception: " << err.what();
  } catch(...) {
    ADD_FAILURE() << "Unexpected unknown exception type";
  }
}

template<typename T>
void expect_future_exception(const pc::shared_future<T>& future, const std::string& what) {
  try {
    const T& unexpected_res = future.get();
    ADD_FAILURE() << "Value " << printable<T>{unexpected_res} << " was returned instead of exception";
  } catch(const std::runtime_error& err) {
    EXPECT_EQ(what, err.what());
  } catch(const std::exception& err) {
    ADD_FAILURE() << "Unexpected exception: " << err.what();
  } catch(...) {
    ADD_FAILURE() << "Unexpected unknown exception type";
  }
}

void expect_future_exception(pc::future<void>& future, const std::string& what);
void expect_future_exception(const pc::shared_future<void>& future, const std::string& what);

#define EXPECT_RUNTIME_ERROR(future, what) expect_future_exception(future, what)
