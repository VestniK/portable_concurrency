#pragma once

#include <portable_concurrency/bits/future.h>
#include <portable_concurrency/bits/shared_future.h>

namespace portable_concurrency {
inline namespace cxx14_v1 {

struct future_get_t {
  template <typename T> decltype(auto) operator()(future<T> &f) const {
    return f.get();
  }

  template <typename T>
  decltype(auto) operator()(const shared_future<T> &f) const {
    return f.get();
  }
};

/**
 * @headerfile portable_concurrency/future
 * @ingroup future_hdr
 * @brief Unary operation which takes a future and returns value stored inside
 * it
 *
 * The main use case for this function object is to transform collection of
 * futures into collection of values. For example:
 * ```
 * auto tasks = run_many_tasks();
 * pc::future<std::vector<int>> = pc::when_all(tasks.begin(), tasks.end())
 *   .next([](std::vector<pc::future<int>> futures){
 *     std::vector<int> results;
 *     results.reserve(futuers.size());
 *     std::transform(futures.begin(), futures.end(),
 * std::back_inserter(results), pc::future_get); return results;
 *   })
 * ```
 */
constexpr future_get_t future_get{};

struct future_ready_t {
  template <typename T> bool operator()(const future<T> &f) const {
    return f.is_ready();
  }

  template <typename T> bool operator()(const shared_future<T> &f) const {
    return f.is_ready();
  }
};

/**
 * @headerfile portable_concurrency/future
 * @ingroup future_hdr
 * @brief Unary predicate which takes a future and returns if this future is
 * ready
 *
 * This predocate can be used with variaty of std algorithms including:
 *  * `std::partition` family
 *  * `std::all_of`, `std::any_of`, `std::none_of`
 *  * `std::find_if`
 *  * `std::count_if`
 */
constexpr future_ready_t future_ready{};

} // namespace cxx14_v1
} // namespace portable_concurrency
