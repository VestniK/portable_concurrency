#pragma once

namespace concurrency {

enum class future_status {
  ready,
  timeout,
  deferred
};

} // namespace CONCURRENCY_NS
