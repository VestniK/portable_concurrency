#pragma once

#include <functional>
#include <iostream>
#include <stdexcept>

#include <gtest/gtest.h>

#include <concurrency/future>

#include "closable_queue.h"
#include "task.h"

#define EXPECT_FUTURE_ERROR(statement, errc) \
  try { \
    statement; \
    ADD_FAILURE() << "Excpected exception was not thrown"; \
  } catch(const std::future_error& err) { \
    EXPECT_EQ(err.code(), errc); \
  } catch(const std::exception& err) { \
    ADD_FAILURE() << "Unexpected exception: " << err.what(); \
  } catch(...) { \
    ADD_FAILURE() << "Unexpected unknown exception type"; \
  }

class future_tests_env: public ::testing::Environment {
public:
  void SetUp() override {
    worker_ = std::thread([](closable_queue<task>& queue) {
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

  void TearDown() override {
    queue_.close();
    if (worker_.joinable())
      worker_.join();
  }

  template<typename F, typename... A>
  void run_async(F&& f, A&&... a) {
    queue_.emplace(std::forward<F>(f), std::forward<A>(a)...);
  }

private:
  std::thread worker_;
  closable_queue<task> queue_;
};

extern
future_tests_env* g_future_tests_env;

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
void expect_future_exception(concurrency::future<T>& future, const std::string& what) {
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

inline
void expect_future_exception(concurrency::future<void>& future, const std::string& what) {
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

// Clang don't want to eat the code bellow:
// template<typename T>
// T some_value() = delete;
// https://llvm.org/bugs/show_bug.cgi?id=17537
// https://llvm.org/bugs/show_bug.cgi?id=18539
template<typename T>
T some_value() {static_assert(sizeof(T) == 0, "some_value<T> is deleted");}

template<>
int some_value<int>() {return 42;}

template<>
std::string some_value<std::string>() {return "hello";}

template<>
std::unique_ptr<int> some_value<std::unique_ptr<int>>() {return std::make_unique<int>(42);}

template<>
future_tests_env& some_value<future_tests_env&>() {return *g_future_tests_env;}
