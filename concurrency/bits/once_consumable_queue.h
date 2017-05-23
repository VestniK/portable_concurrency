#pragma once

#include <atomic>
#include <memory>
#include <type_traits>
#include <utility>

namespace experimental {
inline namespace concurrency_v1 {
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
class forward_list_iterator {
public:
  forward_list_iterator(forward_list_node<T>* node): node_(node) {}

  forward_list_iterator operator++ () {
    if (!node_)
      return *this;
    node_ = node_->next;
    return *this;
  }

  T& operator* () {
    return node_->val;
  }

  bool operator== (const forward_list_iterator& rhs) const {
    return node_ == rhs.node_;
  }

  bool operator!= (const forward_list_iterator& rhs) const {
    return node_ != rhs.node_;
  }

private:
  forward_list_node<T>* node_;
};

template<typename T>
class once_consumable_queue;

template<typename T>
class forward_list {
public:
  forward_list() = default;

  forward_list(const forward_list&) = delete;
  forward_list& operator= (const forward_list&) = delete;

  forward_list(forward_list&& rhs): head_(rhs.head_) {
    rhs.head_ = nullptr;
  }
  forward_list& operator= (forward_list&& rhs) {
    std::swap(head_, rhs.head_);
    return *this;
  }

  ~forward_list() {
    if (!head_)
      return;
    for (auto* p = head_; p != nullptr;) {
      auto* to_delete = p;
      p = p->next;
      delete to_delete;
    }
  }

  forward_list_iterator<T> begin() {
    return {head_};
  }

  forward_list_iterator<T> end() {
    return {nullptr};
  }

public:
  friend class once_consumable_queue<T>;
  forward_list(forward_list_node<T>* head): head_(head) {}

private:
  forward_list_node<T>* head_ = nullptr;
};

/**
 * @internal
 *
 * Multy-producer single-consumer atomic queue with extra properties:
 * @li Consume operation can happen only once and switch the queue into @em consumed state.
 * @li Attempt to enqueue new item into @em consumed queue failes. Item remains unchanged.
 * @li When producer get the information that the queue is @em consumed all side effects
 * of operations sequenced before consume operation in the consumer thread are observable
 * in the thread of this particulair producer.
 *
 * Last requrement allows consumer atomically trasfer data to producers with a single
 * operation of queue consumption.
 */
template<typename T>
class once_consumable_queue {
public:
  once_consumable_queue() = default;
  ~once_consumable_queue() {
    // Create temporary forward_list which can destroy all nodes properly. Nobody
    // else should access this class anymore at the momont of destruction so
    // relaxed memory order is enough.
    auto* head = head_.load(std::memory_order_relaxed);
    if (head && head != consumed_marker())
      forward_list<T>{head};
  }

  /**
   * Enqueue an object in to the queue in a thread safe way. If the queue is not yet
   * @em consumed then this function moves the value from the paremeter @a val and
   * returns true. Otherwise the value of the @a val object remains untoched and
   * function returns false.
   *
   * @note Can be called from multiple threads.
   *
   * @note This function assumes that moving the value from an object and then moving
   * it back remains an object in initial state.
   */
  bool push(T& val) {
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

  /**
   * Clear the queue in a thread safe way. Return false only if the queue is @em consumed
   *
   * @note Can be called from multiple threads.
   */
  bool clear() {
    auto* old_head = head_.load(std::memory_order_acquire);
    do {
      if (old_head == consumed_marker())
        return false;
      if (!old_head)
        return true;
    } while (!head_.compare_exchange_strong(old_head, nullptr, std::memory_order_acq_rel));
    forward_list<T>{old_head}; // Destroys whole list properly
    return true;
  }

  /**
   * Clean the queue and enqueu an object @a val in a thread safe way. If the queue
   * is not yet @em consumed then this function moves the value from the paremeter
   * @a val and returns true. Otherwise the value of the @a val object remains untoched
   * and function returns false.
   *
   * @note Can be called from multiple threads.
   *
   * @note This function assumes that moving the value from an object and then moving
   * it back remains an object in initial state.
   */
  bool replace(T& val) {
    auto* old_head = head_.load(std::memory_order_acquire);
    if (old_head == consumed_marker())
      return false;
    auto new_head = std::make_unique<forward_list_node<T>>(std::move(val));
    while (!head_.compare_exchange_strong(old_head, new_head.get(), std::memory_order_acq_rel)) {
      if (old_head == consumed_marker()) {
        val = std::move(new_head->val);
        return false;
      }
    }
    new_head.release();
    forward_list<T>{old_head}; // Destroys whole list properly
    return true;
  }

  /**
   * Checks if the queue is @em consumed.
   *
   * @note Can be called from multiple threads.
   */
  bool is_consumed() const {
    return head_.load(std::memory_order_acquire) == consumed_marker();
  }

  /**
   * Consumes the queue and switch it into @em consumed state. Running this function
   * concurrently with any other function in this class (instead of constructor or
   * destructor) is safe.
   *
   * @note Must be called from a single thread. Must not be called twice on a same
   * queue.
   */
  forward_list<T> consume() {
    auto* curr_head = head_.exchange(consumed_marker(), std::memory_order_acq_rel);
    if (curr_head == consumed_marker())
      return {};
    return {curr_head};
  }

private:
  // Return address of some valid object which can not alias with forward_list_node<T>
  // instances. Can be used as marker in pointer compariaions but must never be
  // dereferenced.
  forward_list_node<T>* consumed_marker() const {
    return reinterpret_cast<forward_list_node<T>*>(const_cast<once_consumable_queue*>(this));
  }

private:
  std::atomic<forward_list_node<T>*> head_{nullptr};
};

} // inline namespace concurrency_v1
} // namespace detail
} // namespace experimental
