#pragma once

#include <iostream>
#include <stdexcept>
#include <thread>

#include <gtest/gtest.h>

#include <portable_concurrency/functional>
#include <portable_concurrency/future>
#include <portable_concurrency/thread_pool>

#define EXPECT_FUTURE_ERROR(statement, errc)                                   \
  try {                                                                        \
    statement;                                                                 \
    ADD_FAILURE() << "Expected exception was not thrown";                      \
  } catch (const std::future_error &err) {                                     \
    EXPECT_EQ(err.code(), errc)                                                \
        << "received error message: " << err.code().message();                 \
  } catch (const std::exception &err) {                                        \
    ADD_FAILURE() << "Unexpected exception: " << err.what();                   \
  } catch (...) {                                                              \
    ADD_FAILURE() << "Unexpected unknown exception type";                      \
  }

class future_tests_env : public ::testing::Environment {
public:
  explicit future_tests_env(size_t threads_num);

  void TearDown() override;

  template <typename F, typename... A> void run_async(F &&f, A &&...a) {
    post(pool_.executor(),
         pc::detail::make_task(std::forward<F>(f), std::forward<A>(a)...));
  }

  size_t threads_count() const { return tids_.size(); }
  bool uses_thread(std::thread::id id) const;
  void wait_current_tasks();

private:
  pc::static_thread_pool pool_;
  std::vector<std::thread::id> tids_;
};

namespace portable_concurrency {
template <> struct is_executor<future_tests_env *> : std::true_type {};
} // namespace portable_concurrency

template <typename F, typename... A>
void post(future_tests_env *exec, F &&f, A &&...a) {
  exec->run_async(std::forward<F>(f), std::forward<A>(a)...);
}

extern future_tests_env *g_future_tests_env;

struct future_test : ::testing::Test {
  ~future_test();
};

template <typename T> struct printable {
  const T &value;
};

template <typename T>
std::ostream &operator<<(std::ostream &out, const printable<T> &printable) {
  return out << printable.value;
}

std::ostream &operator<<(std::ostream &out,
                         const printable<std::unique_ptr<int>> &printable);
std::ostream &operator<<(std::ostream &out,
                         const printable<future_tests_env &> &printable);

template <typename T>
void expect_future_exception(pc::future<T> &future, const std::string &what) {
  try {
    T unexpected_res = future.get();
    ADD_FAILURE() << "Value " << printable<T>{unexpected_res}
                  << " was returned instead of exception";
  } catch (const std::runtime_error &err) {
    EXPECT_EQ(what, err.what());
  } catch (const std::exception &err) {
    ADD_FAILURE() << "Unexpected exception: " << err.what();
  } catch (...) {
    ADD_FAILURE() << "Unexpected unknown exception type";
  }
}

template <typename T>
void expect_future_exception(const pc::shared_future<T> &future,
                             const std::string &what) {
  try {
    const T &unexpected_res = future.get();
    ADD_FAILURE() << "Value " << printable<T>{unexpected_res}
                  << " was returned instead of exception";
  } catch (const std::runtime_error &err) {
    EXPECT_EQ(what, err.what());
  } catch (const std::exception &err) {
    ADD_FAILURE() << "Unexpected exception: " << err.what();
  } catch (...) {
    ADD_FAILURE() << "Unexpected unknown exception type";
  }
}

void expect_future_exception(pc::future<void> &future, const std::string &what);
void expect_future_exception(const pc::shared_future<void> &future,
                             const std::string &what);

#define EXPECT_RUNTIME_ERROR(future, what) expect_future_exception(future, what)
