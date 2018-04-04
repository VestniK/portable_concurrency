#pragma once

#include <asio/async_result.hpp>
#include <asio/io_context.hpp>
#include <asio/post.hpp>

#include <portable_concurrency/future>
#include <portable_concurrency/execution>

struct use_future_t {};
constexpr use_future_t use_future{};

template<typename R>
struct promise_setter {
  promise_setter(use_future_t) {}

  pc::future<R> get_future() {return promise_.get_future();}

  void operator() (std::error_code ec, const R& val) {
    if (ec)
      promise_.set_exception(std::make_exception_ptr(std::system_error{ec}));
    else
      promise_.set_value(val);
  }

  void operator() (std::error_code ec, R&& val) {
    if (ec)
      promise_.set_exception(std::make_exception_ptr(std::system_error{ec}));
    else
      promise_.set_value(std::move(val));
  }

  pc::promise<R> promise_;
};

template<>
struct promise_setter<void> {
  promise_setter(use_future_t) {}

  pc::future<void> get_future() {return promise_.get_future();}

  void operator() (std::error_code ec) {
    if (ec)
      promise_.set_exception(std::make_exception_ptr(std::system_error{ec}));
    else
      promise_.set_value();
  }

  pc::promise<void> promise_;
};

namespace asio {

template<typename R>
class async_result<::use_future_t, void(std::error_code, R)> {
public:
  using completion_handler_type = ::promise_setter<R>;
  using return_type = pc::future<R>;

  explicit async_result(completion_handler_type& h): res_(h.get_future()) {}
  return_type get() {return std::move(res_);}

private:
  return_type res_;
};

template<>
class async_result<::use_future_t, void(std::error_code)> {
public:
  using completion_handler_type = ::promise_setter<void>;
  using return_type = pc::future<void>;

  explicit async_result(completion_handler_type& h): res_(h.get_future()) {}
  return_type get() {return std::move(res_);}

private:
  return_type res_;
};

} // namespace asio

namespace portable_concurrency {
template<> struct is_executor<asio::io_context::executor_type>: std::true_type {};
} // namespace portable_concurrency
