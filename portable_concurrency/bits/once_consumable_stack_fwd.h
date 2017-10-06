#pragma once

#include <atomic>

namespace portable_concurrency {
inline namespace cxx14_v1 {
namespace detail {

template<typename T>
struct forward_list_node;

template<typename T>
class forward_list_iterator {
public:
  forward_list_iterator(forward_list_node<T>* node): node_(node) {}

  forward_list_iterator operator++ ();

  T& operator* ();

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
class once_consumable_stack;

template<typename T>
class forward_list {
public:
  forward_list() = default;

  forward_list(const forward_list&) = delete;
  forward_list& operator= (const forward_list&) = delete;

  forward_list(forward_list&& rhs);
  forward_list& operator= (forward_list&& rhs);

  ~forward_list();

  forward_list_iterator<T> begin();

  forward_list_iterator<T> end();

  friend class once_consumable_stack<T>;
  forward_list(forward_list_node<T>* head): head_(head) {}

private:
  forward_list_node<T>* head_ = nullptr;
};

/**
 * @internal
 *
 * Multy-producer single-consumer atomic stack with extra properties:
 * @li Consume operation can happen only once and switch the stack into @em consumed state.
 * @li Attempt to push new item into @em consumed stack failes. Item remains unchanged.
 * @li When producer get the information that the stack is @em consumed all side effects
 * of operations sequenced before consume operation in the consumer thread are observable
 * in the thread of this particulair producer.
 *
 * Last requrement allows consumer atomically trasfer data to producers with a single
 * operation of stack consumption.
 */
template<typename T>
class once_consumable_stack {
public:
  once_consumable_stack() = default;
  ~once_consumable_stack();

  /**
   * Push an object to the stack in a thread safe way. If the stack is not yet
   * @em consumed then this function moves the value from the paremeter @a val and
   * returns true. Otherwise the value of the @a val object remains untoched and
   * function returns false.
   *
   * @note Can be called from multiple threads.
   *
   * @note This function assumes that moving the value from an object and then moving
   * it back remains an object in initial state.
   */
  bool push(T& val);

  /**
   * Checks if the stack is @em consumed.
   *
   * @note Can be called from multiple threads.
   */
  bool is_consumed() const;

  /**
   * Consumes the stack and switch it into @em consumed state. Running this function
   * concurrently with any other function in this class (excpet constructor, destructor
   * and consume function itself) is safe.
   *
   * @note Must be called from a single thread. Must not be called twice on a same
   * queue.
   */
  forward_list<T> consume();

private:
  // Return address of some valid object which can not alias with forward_list_node<T>
  // instances. Can be used as marker in pointer compariaions but must never be
  // dereferenced.
  forward_list_node<T>* consumed_marker() const;

private:
  std::atomic<forward_list_node<T>*> head_{nullptr};
};

} // namespace detail
} // inline namespace cxx14_v1
} // namespace portable_concurrency
