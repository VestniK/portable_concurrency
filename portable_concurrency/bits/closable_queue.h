#pragma once

#include <condition_variable>
#include <mutex>
#include <queue>

namespace portable_concurrency {
inline namespace cxx14_v1 {
namespace detail {

template <typename T> class closable_queue {
public:
  bool pop(T &dest);
  void push(T &&val);
  void close();

private:
  std::mutex mutex_;
  std::condition_variable cv_;
  std::queue<T> queue_;
  bool closed_ = false;
};

} // namespace detail
} // namespace cxx14_v1
} // namespace portable_concurrency
