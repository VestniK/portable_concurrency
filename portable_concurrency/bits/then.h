#pragma once

#include "fwd.h"

#include "concurrency_type_traits.h"
#include "execution.h"
#include "shared_state.h"
#include "utils.h"

namespace portable_concurrency {
inline namespace cxx14_v1 {
namespace detail {

struct inplace_executor {};
template<typename F>
void post(inplace_executor, F&& f) {portable_concurrency::detail::invoke(std::forward<F>(f));}

} // namespace detail
} // inline namespace cxx14_v1

template<>
struct is_executor<detail::inplace_executor>: std::true_type {};

inline namespace cxx14_v1 {
namespace detail {

// small helpers

template<typename T>
std::weak_ptr<T> weak(std::shared_ptr<T> ptr) {return {std::move(ptr)};}

template<typename T>
T& unwrapped_ref(T& ref) {return ref;}

template<typename T>
decltype(auto) unwrapped_ref(future<T>& f) {return state_of(f)->value_ref();}

template<typename T>
decltype(auto) unwrapped_ref(shared_future<T>& f) {return state_of(f)->value_ref();}

// Tags for different continuation types and helper type traits to work with them

enum class cnt_tag {
  then,
  next,
  shared_then,
  shared_next
};

template<cnt_tag Type, typename T>
struct cnt_arg;
template<cnt_tag Type, typename T>
using cnt_arg_t = typename cnt_arg<Type, T>::type;

template<typename T>
struct cnt_arg<cnt_tag::then, T> {
  using type = future<T>;
  static type extract(std::shared_ptr<future_state<T>>& state) {return {std::move(state)};}
};
template<typename T>
struct cnt_arg<cnt_tag::next, T> {
  using type = T;
  static type&& extract(std::shared_ptr<future_state<T>>& state) {return std::move(state->value_ref());}
};
template<>
struct cnt_arg<cnt_tag::next, void> {
  using type = void;
  static void extract(std::shared_ptr<future_state<void>>& state) {state->value_ref();}
};
template<typename T>
struct cnt_arg<cnt_tag::shared_then, T> {
  using type = shared_future<T>;
  static type extract(std::shared_ptr<future_state<T>>& state) {return {std::move(state)};}
};
template<typename T>
struct cnt_arg<cnt_tag::shared_next, T> {
  using type = std::add_lvalue_reference_t<std::add_const_t<T>>;
  static const type& extract(std::shared_ptr<future_state<T>>& state) {return state->value_ref();}
};
template<>
struct cnt_arg<cnt_tag::shared_next, void> {
  using type = void;
  static void extract(std::shared_ptr<future_state<void>>& state) {state->value_ref();}
};

// Invoke continuation and emplace result into storage. Separate helpers for cases when continuation argument is
// void/non-void and continuation result is void/non-void.

template<cnt_tag Tag, typename S, typename F, typename T>
auto invoke_emplace(S& storage, F&& func, std::shared_ptr<future_state<T>>) -> std::enable_if_t<
  std::is_void<cnt_arg_t<Tag, T>>::value && !std::is_void<
    decltype(portable_concurrency::detail::invoke(std::forward<F>(func)))
  >::value
> {
  storage.emplace(
    state_t<1>{},
    portable_concurrency::detail::invoke(std::forward<F>(func))
  );
}

template<cnt_tag Tag, typename S, typename F, typename T>
auto invoke_emplace(S& storage, F&& func, std::shared_ptr<future_state<T>>) -> std::enable_if_t<
  std::is_void<cnt_arg_t<Tag, T>>::value && std::is_void<
    decltype(portable_concurrency::detail::invoke(std::forward<F>(func)))
  >::value
> {
  portable_concurrency::detail::invoke(std::forward<F>(func));
  storage.emplace(state_t<1>{});
}

template<cnt_tag Tag, typename S, typename F, typename T>
auto invoke_emplace(S& storage, F&& func, std::shared_ptr<future_state<T>> parent) -> std::enable_if_t<
  !std::is_void<cnt_arg_t<Tag, T>>::value && !std::is_void<
    decltype(portable_concurrency::detail::invoke(std::forward<F>(func), cnt_arg<Tag, T>::extract(parent)))
  >::value
> {
  storage.emplace(
    state_t<1>{},
    portable_concurrency::detail::invoke(std::forward<F>(func), cnt_arg<Tag, T>::extract(parent))
  );
}

template<cnt_tag Tag, typename S, typename F, typename T>
auto invoke_emplace(S& storage, F&& func, std::shared_ptr<future_state<T>> parent) -> std::enable_if_t<
  !std::is_void<cnt_arg_t<Tag, T>>::value && std::is_void<
    decltype(portable_concurrency::detail::invoke(std::forward<F>(func), cnt_arg<Tag, T>::extract(parent)))
  >::value
> {
  portable_concurrency::detail::invoke(std::forward<F>(func), cnt_arg<Tag, T>::extract(parent));
  storage.emplace(state_t<1>{});
}

// continuation state

template<typename E, typename F, typename T>
struct cnt_closure {
  cnt_closure(const E& exec, F&& func, std::shared_ptr<future_state<T>> parent):
    exec(exec), func(std::move(func)), parent(std::move(parent))
  {}

  E exec;
  F func;
  std::shared_ptr<future_state<T>> parent;
};

class continuation_state {
public:
  virtual ~continuation_state() = default;
  virtual void run(std::shared_ptr<continuation_state> self_sp) = 0;
  virtual void abandon() = 0;
};

struct cnt_action {
  std::weak_ptr<continuation_state> wdata;

  cnt_action(const cnt_action&) = delete;

#if defined(__GNUC__) && __GNUC__ < 5
  cnt_action(std::weak_ptr<continuation_state> wdata): wdata(std::move(wdata)) {}
  cnt_action(cnt_action&& rhs) noexcept: wdata(std::move(rhs.wdata)) {rhs.wdata.reset();}
  cnt_action& operator=(cnt_action&& rhs) noexcept {
    wdata = std::move(rhs.wdata);
    rhs.wdata.reset();
    return *this;
  }
#else
  cnt_action(cnt_action&&) noexcept = default;
  cnt_action& operator=(cnt_action&&) noexcept = default;
#endif

  void operator() () {
    if (auto data = wdata.lock())
      data->run(data);
    wdata.reset();
  }

  ~cnt_action() {
    if (auto data = wdata.lock())
      data->abandon();
  }
};

template<cnt_tag Tag, typename T, typename E, typename F>
class cnt_state final:
  public future_state<remove_future_t<cnt_result_t<F, cnt_arg_t<Tag, T>>>>,
  public continuation_state
{
public:
  using cnt_res = cnt_result_t<F, cnt_arg_t<Tag, T>>;
  using res_t = remove_future_t<cnt_res>;
  using stored_cnt_res = std::conditional_t<std::is_void<cnt_res>::value, void_val, cnt_res>;

  cnt_state(const E& exec, F func, std::shared_ptr<future_state<T>>&& parent):
    storage_(state_t<0>{}, exec, std::move(func), std::move(parent))
  {}

  std::add_lvalue_reference_t<state_storage_t<res_t>> value_ref() override {
    if (exception_)
      std::rethrow_exception(exception_);
    return unwrapped_ref(storage_.get(state_t<1>{}));
  }

  std::exception_ptr exception() override {return exception_;}

  void abandon() override {
    if (continuations_.executed())
      return;
    exception_ = std::make_exception_ptr(std::future_error{std::future_errc::broken_promise});
    continuations_.execute();
  }

  void schedule(const std::shared_ptr<cnt_state>& self_sp) {
    assert(self_sp.get() == this);
    E exec = storage_.get(state_t<0>{}).exec;
    post(exec, cnt_action{self_sp});
  }

  void run(std::shared_ptr<continuation_state> self_sp) override try {
#if defined(_MSC_VER)
#pragma warning(push)
// conditional expression is constant
#pragma warning(disable: 4127)
#endif
    auto& action = storage_.get(state_t<0>{});
    if (Tag != cnt_tag::then && Tag != cnt_tag::shared_then) {
      if (auto error = action.parent->exception()) {
        storage_.clean();
        exception_ = std::move(error);
        continuations_.execute();
        return;
      }
    }
#if defined(_MSC_VER)
#pragma warning(pop)
#endif

    invoke_emplace<Tag>(storage_, std::move(action.func), std::move(action.parent));
    unwrap(std::move(self_sp), is_future<cnt_result_t<F, cnt_arg_t<Tag, T>>>{});
  } catch (...) {
    exception_ = std::current_exception();
    storage_.clean();
    continuations_.execute();
  }

  continuations_stack& continuations() final {
    return continuations_;
  }

private:
  auto unwrap(std::shared_ptr<continuation_state>, std::false_type) {
    continuations_.execute();
  }

  auto unwrap(std::shared_ptr<continuation_state> self_sp, std::true_type) {
    auto& res_future = storage_.get(state_t<1>{});
    if (!res_future.valid()) {
      exception_ = std::make_exception_ptr(std::future_error{std::future_errc::broken_promise});
      continuations_.execute();
      return;
    }
    state_of(res_future)->continuations().push([wstate = weak(std::static_pointer_cast<cnt_state>(self_sp))] {
      if (auto state = wstate.lock()) {
        state->continuations_.execute();
      }
    });
  }

private:
  either<cnt_closure<E, F, T>, stored_cnt_res> storage_;
  std::exception_ptr exception_ = nullptr;
  continuations_stack continuations_;
};

template<cnt_tag Tag, typename T, typename E, typename F>
auto make_then_state(std::shared_ptr<future_state<T>> parent, E&& exec, F&& f) {
  using cnt_data_t = cnt_state<Tag, T, std::decay_t<E>, std::decay_t<F>>;
  using res_t = typename cnt_data_t::res_t;

  auto& continuations = parent->continuations();
  auto data = std::make_shared<cnt_data_t>(std::forward<E>(exec), std::forward<F>(f), std::move(parent));
  continuations.push([wdata = weak(data)] {
    if (auto data = wdata.lock())
      data->schedule(data);
  });
  return std::shared_ptr<future_state<res_t>>{data};
}

} // namespace detail
} // inline namespace cxx14_v1
} // namespace portable_concurrency
