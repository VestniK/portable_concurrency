#pragma once

#include <atomic>
#include <memory>
#include <type_traits>
#include <utility>

#include "once_consumable_stack_fwd.h"

namespace portable_concurrency {
inline namespace cxx14_v1 {
namespace detail {

template<typename T>
struct forward_list_node {
  // make_unique can't use universal initialization so some garbage code requred :(
  template<typename U>
  forward_list_node(U&& val, forward_list_node* next = nullptr):
    val(std::forward<U>(val)),
    next(next)
  {}

  T val;
  forward_list_node* next;
};

template<typename T>
void forward_list_deleter<T>::operator() (forward_list_node<T>* head) noexcept {
  if (!head)
    return;
  for (auto* p = head; p != nullptr;) {
    auto* to_delete = p;
    p = p->next;
    delete to_delete;
  }
}

template<typename T>
class forward_list_iterator {
public:
  forward_list_iterator() noexcept = default;
  forward_list_iterator(forward_list<T>& list) noexcept: node_(list.get()) {}

  forward_list_iterator operator++ () noexcept {
    if (!node_)
      return *this;
    node_ = node_->next;
    return *this;
  }

  T& operator* () noexcept {
    return node_->val;
  }

  bool operator== (const forward_list_iterator& rhs) const noexcept {
    return node_ == rhs.node_;
  }

  bool operator!= (const forward_list_iterator& rhs) const noexcept {
    return node_ != rhs.node_;
  }

private:
  forward_list_node<T>* node_ = nullptr;
};

template<typename T>
forward_list_iterator<T> begin(forward_list<T>& list) noexcept {
  return {list};
}

template<typename T>
forward_list_iterator<T> end(forward_list<T>&) noexcept {
  return {};
}

template<typename T>
once_consumable_stack<T>::~once_consumable_stack() {
  // Create temporary forward_list which can destroy all nodes properly. Nobody
  // else should access this object anymore at the momont of destruction so
  // relaxed memory order is enough.
  auto* head = head_.load(std::memory_order_relaxed);
  if (head && head != consumed_marker())
    forward_list<T>{head};
}

template<typename T>
bool once_consumable_stack<T>::push(T& val) {
  auto* curr_head = head_.load(std::memory_order_acquire);
  if (curr_head == consumed_marker())
    return false;
  auto new_head = std::make_unique<forward_list_node<T>>(std::move(val), curr_head);
  while (!head_.compare_exchange_strong(new_head->next, new_head.get(), std::memory_order_acq_rel)) {
    if (new_head->next == consumed_marker()) {
      val = std::move(new_head->val);
      return false;
    }
  }
  new_head.release();
  return true;
}

template<typename T>
bool once_consumable_stack<T>::is_consumed() const noexcept {
  return head_.load(std::memory_order_acquire) == consumed_marker();
}

template<typename T>
forward_list<T> once_consumable_stack<T>::consume() noexcept {
  auto* curr_head = head_.exchange(consumed_marker(), std::memory_order_acq_rel);
  if (curr_head == consumed_marker())
    return {};
  return forward_list<T>{curr_head};
}

template<typename T>
forward_list_node<T>* once_consumable_stack<T>::consumed_marker() const noexcept {
  return reinterpret_cast<forward_list_node<T>*>(const_cast<once_consumable_stack*>(this));
}

} // namespace detail
} // inline namespace cxx14_v1
} // namespace portable_concurrency
