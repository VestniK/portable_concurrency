#pragma once

#include <cassert>
#include <type_traits>
#include <utility>

#include "once_consumable_stack.h"

namespace portable_concurrency {
inline namespace cxx14_v1 {
namespace detail {

template <typename T>
void forward_list_deleter<T>::operator()(forward_list_node<T> *head) noexcept {
  if (!head)
    return;
  for (auto *p = head; p != nullptr;) {
    auto *to_delete = p;
    p = p->next;
    to_delete->deallocate_self();
  }
}

template <typename T> class forward_list_iterator {
public:
  forward_list_iterator() noexcept = default;
  forward_list_iterator(forward_list<T> &list) noexcept : node_(list.get()) {}

  forward_list_iterator operator++() noexcept {
    if (!node_)
      return *this;
    node_ = node_->next;
    return *this;
  }

  T &operator*() noexcept { return node_->val; }

  bool operator==(const forward_list_iterator &rhs) const noexcept {
    return node_ == rhs.node_;
  }

  bool operator!=(const forward_list_iterator &rhs) const noexcept {
    return node_ != rhs.node_;
  }

private:
  forward_list_node<T> *node_ = nullptr;
};

template <typename T>
forward_list_iterator<T> begin(forward_list<T> &list) noexcept {
  return {list};
}

template <typename T> forward_list_iterator<T> end(forward_list<T> &) noexcept {
  return {};
}

template <typename T>
once_consumable_stack<T>::once_consumable_stack() noexcept {}

template <typename T> once_consumable_stack<T>::~once_consumable_stack() {
  // Create temporary forward_list which can destroy all nodes properly. Nobody
  // else should access this object anymore at the momont of destruction so
  // relaxed memory order is enough.
  auto *head = head_.load(std::memory_order_relaxed);
  if (head && head != consumed_marker())
    forward_list<T>{head};
}

template <typename T> bool once_consumable_stack<T>::push(T &val) {
  return push(val, std::allocator<T>{});
}

template <typename T>
bool once_consumable_stack<T>::push(forward_list<T> &node) noexcept {
  assert(!node->next);
  auto *curr_head = head_.load(std::memory_order_acquire);
  if (curr_head == consumed_marker())
    return false;
  node->next = curr_head;
  while (!head_.compare_exchange_strong(node->next, node.get(),
                                        std::memory_order_acq_rel)) {
    if (node->next == consumed_marker()) {
      node->next = nullptr;
      return false;
    }
  }
  node.release();
  return true;
}

template <typename T>
bool once_consumable_stack<T>::is_consumed() const noexcept {
  return head_.load(std::memory_order_acquire) == consumed_marker();
}

template <typename T>
forward_list<T> once_consumable_stack<T>::consume() noexcept {
  auto *curr_head =
      head_.exchange(consumed_marker(), std::memory_order_acq_rel);
  if (curr_head == consumed_marker())
    return {nullptr, forward_list_deleter<T>{}};
  return forward_list<T>{curr_head, forward_list_deleter<T>{}};
}

template <typename T>
forward_list_node<T> *
once_consumable_stack<T>::consumed_marker() const noexcept {
  return reinterpret_cast<forward_list_node<T> *>(
      const_cast<once_consumable_stack *>(this));
}

} // namespace detail
} // namespace cxx14_v1
} // namespace portable_concurrency
