#pragma once

#include <stdexcept>
#include <system_error>

namespace concurrency {

const std::error_category& future_category() noexcept;

enum class future_errc {
  broken_promise = 1,
  future_already_retrieved,
  promise_already_satisfied,
  no_state
};
std::error_code make_error_code(future_errc errc);
std::error_condition make_error_condition(future_errc errc);

class future_error: public std::logic_error {
public:
  explicit future_error(future_errc errc);

  const char* what() const noexcept override;
  const std::error_code& code() const noexcept {return ec_;}

private:
  std::error_code ec_;
};

} // namespace concurrency

namespace std {

template<>
struct is_error_code_enum<concurrency::future_errc>: std::true_type {};

} // namespace std
