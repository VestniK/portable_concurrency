#pragma once

#include <iostream>
#include <stdexcept>

#include <gtest/gtest.h>

#include <concurrency/future>

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
