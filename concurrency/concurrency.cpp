#include <string>

#include "future"

namespace concurrency {
namespace {

struct future_category_t: std::error_category {
  const char* name() const noexcept override {return "future";}

  std::string message(int condition) const override {
    switch(static_cast<future_errc>(condition)) {
      case future_errc::broken_promise:
        return "the asynchronous task abandoned its shared state";
      case future_errc::future_already_retrieved:
        return "the contents of shared state were already accessed through a future object";
      case future_errc::promise_already_satisfied:
        return "attempt to store a value in the shared state twice";
      case future_errc::no_state:
        return "attempt to access promise or future without an associated shared state";
    }
    return "invalid future error code";
  }
};

} // anonymous namespace

const std::error_category& future_category() noexcept {
  static const future_category_t inst = {};
  return inst;
}

std::error_code make_error_code(future_errc errc) {
  return {static_cast<int>(errc), future_category()};
}

std::error_condition make_error_condition(future_errc errc)
{
  return {static_cast<int>(errc), future_category()};
}

future_error::future_error(future_errc errc):
  std::logic_error(("future_error: " + future_category().message(static_cast<int>(errc))).c_str()),
  ec_(errc) {}

} // namespace concurrency
