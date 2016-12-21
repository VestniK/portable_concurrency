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
