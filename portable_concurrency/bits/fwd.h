#pragma once

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

} // inline namespace cxx14_v1
} // namespace portable_concurrency
