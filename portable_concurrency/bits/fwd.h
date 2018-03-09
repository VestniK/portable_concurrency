#pragma once

#include <memory>

namespace portable_concurrency {
inline namespace cxx14_v1 {

enum class future_status {
  ready,
  timeout
};

template<typename T> class future;
template<typename Signature> class packaged_task;
template<typename T> class promise;
template<typename T> class shared_future;

namespace detail {
template<typename T> class future_state;

template<typename T>
std::shared_ptr<future_state<T>>& state_of(future<T>&);
template<typename T>
std::shared_ptr<future_state<T>> state_of(future<T>&&);

template<typename T>
std::shared_ptr<future_state<T>>& state_of(shared_future<T>&);
template<typename T>
std::shared_ptr<future_state<T>> state_of(shared_future<T>&&);
} // namespace detail

} // inline namespace cxx14_v1
} // namespace portable_concurrency
