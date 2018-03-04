#pragma once

#include <cassert>
#include <exception>
#include <future>
#include <type_traits>

#include "fwd.h"
#include "either.h"
#include "future_state.h"

namespace portable_concurrency {
inline namespace cxx14_v1 {
namespace detail {

template<typename T>
class shared_state: public future_state<T> {
public:
  shared_state() = default;

  shared_state(const shared_state&) = delete;
  shared_state(shared_state&&) = delete;

  template<typename... U>
  void emplace(U&&... u) {
    if (!storage_.empty())
      throw std::future_error(std::future_errc::promise_already_satisfied);
    storage_.emplace(state_t<0>{}, std::forward<U>(u)...);
    this->continuations().execute();
  }

  void set_exception(std::exception_ptr error) {
    if (!storage_.empty())
      throw std::future_error(std::future_errc::promise_already_satisfied);
    storage_.emplace(state_t<2>{}, error);
    this->continuations().execute();
  }

  state_storage_t<T>& value_ref() final {
    assert(this->continuations().executed());
    struct {
      state_storage_t<T>& operator() (state_storage_t<T>& val) const {return val;}
      state_storage_t<T>& operator() (std::shared_ptr<future_state<T>>& val) const {return val->value_ref();}
      state_storage_t<T>& operator() (std::exception_ptr& err) const {std::rethrow_exception(err);}
      state_storage_t<T>& operator() (monostate) {
        assert(false);
        throw std::logic_error{"Attempt to access shared_state storage while it's empty"};
      }
    } visitor;
    return storage_.visit(visitor);
  }

  std::exception_ptr exception() final {
    assert(this->continuations().executed());
    struct {
      std::exception_ptr operator() (state_storage_t<T>&) const {return nullptr;}
      std::exception_ptr operator() (std::shared_ptr<future_state<T>>& val) const {return val->exception();}
      std::exception_ptr operator() (std::exception_ptr& err) const {return err;}
      std::exception_ptr operator() (monostate) {
        assert(false);
        throw std::logic_error{"Attempt to access shared_state storage while it's empty"};
      }
    } visitor;
    return storage_.visit(visitor);
  }

  continuations_stack& continuations() final {return continuations_;}

private:
  either<state_storage_t<T>, std::shared_ptr<future_state<T>>, std::exception_ptr> storage_;
  continuations_stack continuations_;
};

template<typename T, typename Alloc>
class allocated_state final: private Alloc, public shared_state<T> {
public:
  allocated_state(const Alloc& allocator): Alloc(allocator) {}

  void push(continuation&& cnt) final {
    static_cast<shared_state<T>*>(this)->continuations().push(std::move(cnt), get_allocator());
  }

private:
  Alloc& get_allocator() {return *this;}
};

} // namespace detail
} // inline namespace cxx14_v1
} // namespace portable_concurrency
