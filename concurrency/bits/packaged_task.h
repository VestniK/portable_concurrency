#pragma once

#include <memory>

#include "fwd.h"

#include "future.h"
#include "shared_state.h"
#include "utils.h"

namespace experimental {
inline namespace concurrency_v1 {

namespace detail {
template<typename R, typename... A>
class task_state: public shared_state<R> {
public:
  virtual void invoke(A&&...) noexcept = 0;
};

} // namespace detail

template<typename R, typename... A>
class packaged_task<R(A...)> {
public:
  packaged_task() noexcept = default;

  packaged_task(const packaged_task&) = delete;
  packaged_task(packaged_task&&) noexcept = default;

  packaged_task& operator= (const packaged_task&) = delete;
  packaged_task& operator= (packaged_task&&) noexcept = default;

  bool valid() const noexcept {return static_cast<bool>(state_);}

  void swap(packaged_task& other) noexcept {state_.swap(other.state_);}

  future<R> get_future() {
    if (!state_)
      throw std::future_error(std::future_errc::no_state);
    if (state_.use_count() != 1)
      throw std::future_error(std::future_errc::future_already_retrieved);
    return detail::make_future(detail::decay_copy(state_));
  }

  void operator() (A... a) {
    if (!state_)
      throw std::future_error(std::future_errc::no_state);
    state_->invoke(a...);
  }

  void reset() {} // TODO

private:
  std::shared_ptr<detail::task_state<R, A...>> state_;
};

} // inline namespace concurrency_v1
} // namespace experimental

