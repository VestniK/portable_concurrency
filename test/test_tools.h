#pragma once

#include <functional>
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
    ADD_FAILURE() << "Excpected exception was not thrown"; \
  } catch(const std::future_error& err) { \
    EXPECT_EQ(err.code(), errc) << "received error message: " << err.code().message(); \
  } catch(const std::exception& err) { \
    ADD_FAILURE() << "Unexpected exception: " << err.what(); \
  } catch(...) { \
    ADD_FAILURE() << "Unexpected unknown exception type"; \
  }

class future_tests_env: public ::testing::Environment {
public:
  void SetUp() override;

  void TearDown() override;

  template<typename F, typename... A>
  void run_async(F&& f, A&&... a) {
    queue_.push(make_task(std::forward<F>(f), std::forward<A>(a)...));
  }

  size_t threads_count() const {return workers_.size();}

  bool uses_thread(std::thread::id id) const {
    return std::find_if(
      workers_.begin(), workers_.end(), [id](const std::thread& t) {return t.get_id() == id;}
    ) != workers_.end();
  }

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
  ~future_test() {g_future_tests_env->wait_current_tasks();}
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

inline
std::ostream& operator<< (std::ostream& out, const printable<std::unique_ptr<int>>& printable) {
  return out << *(printable.value);
}

inline
std::ostream& operator<< (std::ostream& out, const printable<future_tests_env&>& printable) {
  return out << "future_tests_env instance: 0x" << std::hex << reinterpret_cast<std::uintptr_t>(&printable.value);
}

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

inline
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

inline
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

#define EXPECT_RUNTIME_ERROR(future, what) expect_future_exception(future, what)
