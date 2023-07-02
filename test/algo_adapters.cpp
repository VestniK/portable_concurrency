#include <algorithm>
#include <vector>

#include <gtest/gtest.h>

#include <portable_concurrency/future>

namespace {

template <template <class...> class Future>
std::vector<Future<int>> make_test_futures(std::initializer_list<int> values) {
  std::vector<Future<int>> futures;
  for (int val : values)
    futures.push_back(pc::make_ready_future(val));
  return futures;
}

TEST(future_get_transformation, extract_values_from_futures) {
  std::vector<pc::future<int>> futures =
      make_test_futures<pc::future>({100, 500, 42, 0xff});

  std::vector<int> values;
  values.reserve(futures.size());
  std::transform(futures.begin(), futures.end(), std::back_inserter(values),
                 pc::future_get);
  EXPECT_EQ(values, (std::vector<int>{100, 500, 42, 0xff}));
}

TEST(future_get_transformation, extract_values_from_shared_futures) {
  std::vector<pc::shared_future<int>> futures =
      make_test_futures<pc::shared_future>({100, 500, 42, 0xff});

  std::vector<int> values;
  values.reserve(futures.size());
  std::transform(futures.begin(), futures.end(), std::back_inserter(values),
                 pc::future_get);
  EXPECT_EQ(values, (std::vector<int>{100, 500, 42, 0xff}));
}

} // namespace
