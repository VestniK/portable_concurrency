#pragma once

#include <atomic>
#include <memory>
#include <type_traits>
#include <utility>

#include "allocate_unique.h"
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

template<typename T, typename Alloc>
void forward_list_deleter<T, Alloc>::operator() (forward_list_node<T>* head) noexcept {
  if (!head)
    return;
  for (auto* p = head; p != nullptr;) {
    auto* to_delete = p;
    p = p->next;
    std::allocator_traits<Alloc>::destroy(allocator, to_delete);
    std::allocator_traits<Alloc>::deallocate(allocator, to_delete, 1);
  }
}

template<typename T>
class forward_list_iterator {
public:
  forward_list_iterator() noexcept = default;
  template<typename Alloc>
  forward_list_iterator(forward_list<T, Alloc>& list) noexcept: node_(list.get()) {}

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

template<typename T, typename Alloc>
forward_list_iterator<T> begin(forward_list<T, Alloc>& list) noexcept {
  return {list};
}

template<typename T, typename Alloc>
forward_list_iterator<T> end(forward_list<T, Alloc>&) noexcept {
  return {};
}

template<typename T, typename Alloc>
once_consumable_stack<T, Alloc>::once_consumable_stack(const Alloc& allocator) noexcept :
    allocator_(allocator)
{ }

template<typename T, typename Alloc>
once_consumable_stack<T, Alloc>::~once_consumable_stack() {
  // Create temporary forward_list which can destroy all nodes properly. Nobody
  // else should access this object anymore at the momont of destruction so
  // relaxed memory order is enough.
  auto* head = head_.load(std::memory_order_relaxed);
  if (head && head != consumed_marker())
    forward_list<T, Alloc>{head, forward_list_deleter<T, Alloc>(get_allocator())};
}

template<typename T, typename Alloc>
bool once_consumable_stack<T, Alloc>::push(T& val) {
  auto* curr_head = head_.load(std::memory_order_acquire);
  if (curr_head == consumed_marker())
    return false;
  auto new_head = allocate_unique<forward_list_node<T>>(allocator_, std::move(val), curr_head);
  while (!head_.compare_exchange_strong(new_head->next, new_head.get(), std::memory_order_acq_rel)) {
    if (new_head->next == consumed_marker()) {
      val = std::move(new_head->val);
      return false;
    }
  }
  new_head.release();
  return true;
}

template<typename T, typename Alloc>
bool once_consumable_stack<T, Alloc>::is_consumed() const noexcept {
  return head_.load(std::memory_order_acquire) == consumed_marker();
}

template<typename T, typename Alloc>
forward_list<T, Alloc> once_consumable_stack<T, Alloc>::consume() noexcept {
  auto* curr_head = head_.exchange(consumed_marker(), std::memory_order_acq_rel);
  if (curr_head == consumed_marker())
    return {nullptr, forward_list_deleter<T, Alloc>(allocator_)};
  return forward_list<T, Alloc>{curr_head, forward_list_deleter<T, Alloc>(allocator_)};
}

template<typename T, typename Alloc>
Alloc once_consumable_stack<T, Alloc>::get_allocator() const {
  return allocator_;
}

template<typename T, typename Alloc>
forward_list_node<T>* once_consumable_stack<T, Alloc>::consumed_marker() const noexcept {
  return reinterpret_cast<forward_list_node<T>*>(const_cast<once_consumable_stack*>(this));
}

} // namespace detail
} // inline namespace cxx14_v1
} // namespace portable_concurrency
