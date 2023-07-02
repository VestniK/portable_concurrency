#pragma once

#include <memory>
#include <utility>

#include "coro.h"
#include "either.h"
#include "future.h"
#include "shared_state.h"

namespace portable_concurrency {
inline namespace cxx14_v1 {

namespace detail {

template <typename T, typename F>
class cancellable_state final : public shared_state<T> {
public:
  cancellable_state(const cancellable_state &) = delete;
  cancellable_state &operator=(const cancellable_state &) = delete;
  cancellable_state(cancellable_state &&) = delete;
  cancellable_state &operator=(cancellable_state &&) = delete;

  ~cancellable_state() noexcept {
    if (!this->continuations().executed())
      cancel_action();
  }

  cancellable_state(const F &action)
      : shared_state<T>{}, cancel_action{action} {}
  cancellable_state(F &&action)
      : shared_state<T>{}, cancel_action{std::move(action)} {}

private:
  F cancel_action;
};

template <typename T> struct promise_common {
  either<detail::monostate, std::shared_ptr<shared_state<T>>,
         std::weak_ptr<shared_state<T>>>
      state_;

  promise_common()
      : state_{in_place_index_t<1>{}, std::make_shared<shared_state<T>>()} {}
  template <typename Alloc>
  explicit promise_common(const Alloc &allocator)
      : state_{in_place_index_t<1>{},
               std::allocate_shared<allocated_state<T, Alloc>>(allocator,
                                                               allocator)} {}
  template <typename F>
  promise_common(canceler_arg_t, F &&f)
      : state_{in_place_index_t<1>{},
               std::make_shared<cancellable_state<T, std::decay_t<F>>>(
                   std::forward<F>(f))} {}

  promise_common(std::weak_ptr<detail::shared_state<T>> &&state)
      : state_{in_place_index_t<2>{}, std::move(state)} {}

  ~promise_common() { abandon(); }

  promise_common(promise_common &&) noexcept = default;
  promise_common &operator=(promise_common &&rhs) noexcept {
    abandon();
    state_ = std::move(rhs.state_);
    return *this;
  }

  void abandon() {
    struct {
      void operator()(const std::weak_ptr<shared_state<T>> &wstate) {
        if (auto state = wstate.lock())
          state->abandon();
      }
      void operator()(const std::shared_ptr<shared_state<T>> &) {}
      void operator()(monostate) {}
    } visitor;
    state_.visit(visitor);
  }

  future<T> get_future() {
    if (state_.state() == 2u)
      throw_already_retrieved();
    auto state = get_state();
    state_.emplace(in_place_index_t<2>{}, state);
    return {state};
  }

  void set_exception(std::exception_ptr error) {
    auto state = get_state();
    if (state)
      state->set_exception(error);
  }

  std::shared_ptr<shared_state<T>> get_state() {
    struct {
      std::shared_ptr<shared_state<T>>
      operator()(const std::shared_ptr<shared_state<T>> &val) const {
        return val;
      }
      std::shared_ptr<shared_state<T>>
      operator()(const std::weak_ptr<shared_state<T>> &val) const {
        return val.lock();
      }
      std::shared_ptr<shared_state<T>> operator()(monostate) {
        throw_no_state();
      }
    } visitor;
    return state_.visit(visitor);
  }

  bool is_awaiten() const {
    struct {
      bool operator()(const std::shared_ptr<shared_state<T>> &) const {
        return true;
      }
      bool operator()(const std::weak_ptr<shared_state<T>> &val) const {
        return !val.expired();
      }
      bool operator()(monostate) const { throw_no_state(); }
    } visitor;
    return state_.visit(visitor);
  }
};

} // namespace detail

/**
 * @ingroup future_hdr
 * @brief The class template promise is a simplest write end of a future.
 */
template <typename T> class promise {
public:
  promise() = default;
  template <typename Alloc>
  promise(std::allocator_arg_t, const Alloc &allocator = Alloc())
      : common_(allocator) {}
  template <typename F>
  promise(canceler_arg_t tag, F &&f) : common_{tag, std::forward<F>(f)} {}

  promise(std::weak_ptr<detail::shared_state<T>> &&state)
      : common_(std::move(state)) {}

  promise(promise &&) noexcept = default;
  promise(const promise &) = delete;

  promise &operator=(const promise &) = delete;
  promise &operator=(promise &&rhs) noexcept = default;

  ~promise() = default;

  void set_value(const T &val) {
    auto state = common_.get_state();
    if (state)
      state->emplace(val);
  }

  void set_value(T &&val) {
    auto state = common_.get_state();
    if (state)
      state->emplace(std::move(val));
  }

#if !defined(PC_NO_DEPRECATED)
  [[deprecated("Use pc::make_promise instead")]] future<T> get_future() {
    return common_.get_future();
  }
#endif
  void set_exception(std::exception_ptr error) { common_.set_exception(error); }

  /**
   * Checks if there is a @ref future or @ref shared_future awaiting for the
   * result from this promise object. This method returns `false` only if there
   * is absolutely no way to get a @ref future or @ref shared_future object
   * which can be used to retreive a value or exception set by this promise
   * object.
   */
  bool is_awaiten() const { return common_.is_awaiten(); }

#if defined(PC_HAS_COROUTINES)
  detail::suspend_never initial_suspend() const noexcept { return {}; }
  detail::suspend_never final_suspend() const noexcept { return {}; }
  auto get_return_object() { return common_.get_future(); }
  void unhandled_exception() { set_exception(std::current_exception()); }
  void return_value(const T &val) { set_value(val); }
  void return_value(T &&val) { set_value(std::move(val)); }
#endif

private:
  detail::promise_common<T> common_;
};

template <typename T> class promise<T &> {
public:
  promise() = default;
  template <typename Alloc>
  promise(std::allocator_arg_t, const Alloc &allocator = Alloc())
      : common_(allocator) {}
  template <typename F>
  promise(canceler_arg_t tag, F &&f) : common_{tag, std::forward<F>(f)} {}

  promise(std::weak_ptr<detail::shared_state<T &>> &&state)
      : common_(std::move(state)) {}

  promise(promise &&) noexcept = default;
  promise(const promise &) = delete;

  promise &operator=(const promise &) = delete;
  promise &operator=(promise &&rhs) noexcept = default;

  ~promise() = default;

  void set_value(T &val) {
    auto state = common_.get_state();
    if (state)
      state->emplace(val);
  }

#if !defined(PC_NO_DEPRECATED)
  [[deprecated("Use pc::make_promise instead")]] future<T &> get_future() {
    return common_.get_future();
  }
#endif
  void set_exception(std::exception_ptr error) { common_.set_exception(error); }

  bool is_awaiten() const { return common_.is_awaiten(); }

#if defined(PC_HAS_COROUTINES)
  detail::suspend_never initial_suspend() const noexcept { return {}; }
  detail::suspend_never final_suspend() const noexcept { return {}; }
  auto get_return_object() { return common_.get_future(); }
  void unhandled_exception() { set_exception(std::current_exception()); }
  void return_value(T &val) { set_value(val); }
#endif

private:
  detail::promise_common<T &> common_;
};

template <> class promise<void> {
public:
  promise() = default;
  template <typename Alloc>
  promise(std::allocator_arg_t, const Alloc &allocator = Alloc())
      : common_(allocator) {}
  template <typename F>
  promise(canceler_arg_t tag, F &&f) : common_{tag, std::forward<F>(f)} {}

  promise(std::weak_ptr<detail::shared_state<void>> &&state)
      : common_(std::move(state)) {}

  promise(promise &&) noexcept = default;
  promise(const promise &) = delete;

  promise &operator=(const promise &) = delete;
  promise &operator=(promise &&rhs) noexcept = default;

  ~promise() = default;

  void set_value() {
    auto state = common_.get_state();
    if (state)
      state->emplace();
  }

#if !defined(PC_NO_DEPRECATED)
  [[deprecated("Use pc::make_promise instead")]] future<void> get_future() {
    return common_.get_future();
  }
#endif
  void set_exception(std::exception_ptr error) { common_.set_exception(error); }

  bool is_awaiten() const { return common_.is_awaiten(); }

#if defined(PC_HAS_COROUTINES)
  detail::suspend_never initial_suspend() const noexcept { return {}; }
  detail::suspend_never final_suspend() const noexcept { return {}; }
  auto get_return_object() { return common_.get_future(); }
  void unhandled_exception() { set_exception(std::current_exception()); }
  void return_void() { set_value(); }
#endif

private:
  detail::promise_common<void> common_;
};

template <typename T>
PC_NODISCARD std::pair<promise<T>, future<T>> make_promise() {
  auto state = std::make_shared<detail::shared_state<T>>();
  auto state_weak = std::weak_ptr<detail::shared_state<T>>(state);
  return {promise<T>(std::move(state_weak)), future<T>(std::move(state))};
}

template <typename T, typename Alloc>
PC_NODISCARD std::pair<promise<T>, future<T>>
make_promise(const Alloc &allocator) {
  auto state = std::allocate_shared<detail::allocated_state<T, Alloc>>(
      allocator, allocator);
  auto state_weak = std::weak_ptr<detail::shared_state<T>>(state);
  return {promise<T>(std::move(state_weak)), future<T>(std::move(state))};
}

template <typename T, typename F>
PC_NODISCARD std::pair<promise<T>, future<T>> make_promise(canceler_arg_t,
                                                           F &&f) {
  auto state = std::make_shared<detail::cancellable_state<T, std::decay_t<F>>>(
      std::forward<F>(f));
  auto state_weak = std::weak_ptr<detail::shared_state<T>>(state);
  return {promise<T>(std::move(state_weak)), future<T>(std::move(state))};
}

} // namespace cxx14_v1
} // namespace portable_concurrency
