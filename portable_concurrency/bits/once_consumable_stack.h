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
forward_list_iterator<T> forward_list_iterator<T>::operator++ () {
  if (!node_)
    return *this;
  node_ = node_->next;
  return *this;
}

template<typename T>
T& forward_list_iterator<T>::operator* () {
  return node_->val;
}

template<typename T>
forward_list<T>::forward_list(forward_list&& rhs): head_(rhs.head_) {
  rhs.head_ = nullptr;
}

template<typename T>
forward_list<T>& forward_list<T>::operator= (forward_list<T>&& rhs) {
  std::swap(head_, rhs.head_);
  return *this;
}

template<typename T>
forward_list<T>::~forward_list() {
  if (!head_)
    return;
  for (auto* p = head_; p != nullptr;) {
    auto* to_delete = p;
    p = p->next;
    delete to_delete;
  }
}

template<typename T>
forward_list_iterator<T> forward_list<T>::begin() {
  return {head_};
}

template<typename T>
forward_list_iterator<T> forward_list<T>::end() {
  return {nullptr};
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
bool once_consumable_stack<T>::is_consumed() const {
  return head_.load(std::memory_order_acquire) == consumed_marker();
}

template<typename T>
forward_list<T> once_consumable_stack<T>::consume() {
  auto* curr_head = head_.exchange(consumed_marker(), std::memory_order_acq_rel);
  if (curr_head == consumed_marker())
    return {};
  return {curr_head};
}

template<typename T>
forward_list_node<T>* once_consumable_stack<T>::consumed_marker() const {
  return reinterpret_cast<forward_list_node<T>*>(const_cast<once_consumable_stack*>(this));
}

} // namespace detail
} // inline namespace cxx14_v1
} // namespace portable_concurrency
