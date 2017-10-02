#include "shared_state.h"

namespace portable_concurrency {
inline namespace cxx14_v1 {
namespace detail {

bool continuations_stack::push(value_type& cnt) {
  return stack_.push(cnt);
}

forward_list<continuations_stack::value_type> continuations_stack::consume() {
  return stack_.consume();
}

bool continuations_stack::is_consumed() const {
  return stack_.is_consumed();
}

wait_continuation& continuations_stack::get_waiter() {
  std::call_once(waiter_init_, [this] {
    waiter_ = std::make_unique<wait_continuation>();
    unique_function<void()> cnt = std::ref(*waiter_);
    if (!stack_.push(cnt))
      cnt();
  });
  return *waiter_;
}

template class shared_state<void>;

} // namespace detail
} // inline namespace cxx14_v1
} // namespace portable_concurrency
