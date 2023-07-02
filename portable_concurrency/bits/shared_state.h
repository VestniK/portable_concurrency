#pragma once

#include <cassert>
#include <exception>
#include <stdexcept>
#include <type_traits>

#include "either.h"
#include "future_state.h"
#include "fwd.h"

namespace portable_concurrency {
inline namespace cxx14_v1 {
namespace detail {

[[noreturn]] void throw_no_state();
[[noreturn]] void throw_already_satisfied();
[[noreturn]] void throw_already_retrieved();
std::exception_ptr make_broken_promise();

template <typename T> class shared_state : public future_state<T> {
public:
  shared_state() = default;

  shared_state(const shared_state &) = delete;
  shared_state(shared_state &&) = delete;

  template <typename... U> void emplace(U &&...u) {
    if (!storage_.empty())
      throw_already_satisfied();
    storage_.emplace(in_place_index_t<1>{}, std::forward<U>(u)...);
    this->continuations().execute();
  }

  void set_exception(std::exception_ptr error) {
    if (!storage_.empty())
      throw_already_satisfied();
    storage_.emplace(in_place_index_t<3>{}, error);
    this->continuations().execute();
  }

  void abandon() {
    // In case of unwrap state == 2 (storage holds shared_ptr<shared_state<T>>)
    // and continuations will be executed when storesd state is fulfilled.
    if (!continuations().executed() && storage_.state() != 2)
      set_exception(make_broken_promise());
  }

  state_storage_t<T> &value_ref() final {
    assert(this->continuations().executed());
    struct {
      state_storage_t<T> &operator()(state_storage_t<T> &val) const {
        return val;
      }
      state_storage_t<T> &
      operator()(std::shared_ptr<future_state<T>> &val) const {
        return val->value_ref();
      }
      state_storage_t<T> &operator()(std::exception_ptr &err) const {
        std::rethrow_exception(err);
      }
      state_storage_t<T> &operator()(monostate) {
        assert(false);
        throw std::logic_error{
            "Attempt to access shared_state storage while it's empty"};
      }
    } visitor;
    return storage_.visit(visitor);
  }

  std::exception_ptr exception() final {
    assert(this->continuations().executed());
    struct {
      std::exception_ptr operator()(state_storage_t<T> &) const {
        return nullptr;
      }
      std::exception_ptr
      operator()(std::shared_ptr<future_state<T>> &val) const {
        return val->exception();
      }
      std::exception_ptr operator()(std::exception_ptr &err) const {
        return err;
      }
      std::exception_ptr operator()(monostate) {
        assert(false);
        throw std::logic_error{
            "Attempt to access shared_state storage while it's empty"};
      }
    } visitor;
    return storage_.visit(visitor);
  }

  continuations_stack &continuations() final { return continuations_; }

  static void unwrap(std::shared_ptr<shared_state> &self,
                     const std::shared_ptr<future_state<T>> &val) {
    assert(self);
    if (!val) {
      self->set_exception(make_broken_promise());
      return;
    }
    self->storage_.emplace(in_place_index_t<2>{}, val);
    val->continuations().push([wself = std::weak_ptr<shared_state>(self)] {
      if (auto self = wself.lock())
        self->continuations().execute();
    });
  }

  static void unwrap(std::shared_ptr<shared_state> &self, future<T> &&val) {
    unwrap(self, state_of(std::move(val)));
  }

  static void unwrap(std::shared_ptr<shared_state> &self,
                     shared_future<T> &&val) {
    unwrap(self, state_of(std::move(val)));
  }

  template <typename U>
  static std::enable_if_t<
      !std::is_same<std::decay_t<U>, std::shared_ptr<future_state<T>>>::value>
  unwrap(std::shared_ptr<shared_state> &self, U &&val) {
    self->emplace(std::forward<U>(val));
  }

private:
  either<monostate, state_storage_t<T>, std::shared_ptr<future_state<T>>,
         std::exception_ptr>
      storage_;
  continuations_stack continuations_;
};

template <typename T, typename Alloc>
class allocated_state final : private Alloc, public shared_state<T> {
public:
  allocated_state(const Alloc &allocator) : Alloc(allocator) {}

  void push(continuation &&cnt) final {
    static_cast<shared_state<T> *>(this)->continuations().push(std::move(cnt),
                                                               get_allocator());
  }

private:
  Alloc &get_allocator() { return *this; }
};

} // namespace detail
} // namespace cxx14_v1
} // namespace portable_concurrency
