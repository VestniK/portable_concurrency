#pragma once

#include "fwd.h"

#include "concurrency_type_traits.h"
#include "shared_state.h"
#include "utils.h"

namespace portable_concurrency {
inline namespace cxx14_v1 {
namespace detail {

template<typename T>
std::weak_ptr<T> weak(std::shared_ptr<T> ptr) {return {std::move(ptr)};}

template<typename T>
T& unwrapped_ref(T& ref) {return ref;}

template<typename T>
decltype(auto) unwrapped_ref(future<T>& f) {return state_of(f)->value_ref();}

template<typename T>
decltype(auto) unwrapped_ref(shared_future<T>& f) {return state_of(f)->value_ref();}

enum class cnt_tag {
  then,
  next,
  shared_then
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

template<typename F, typename T>
struct cnt_closure {
  F func;
  std::shared_ptr<future_state<T>> parent;
};

template<cnt_tag Tag, typename T, typename F>
class cnt_data:
  public future_state<remove_future_t<cnt_result_t<F, cnt_arg_t<Tag, T>>>>
{
public:
  using cnt_res = cnt_result_t<F, cnt_arg_t<Tag, T>>;
  using res_t = remove_future_t<cnt_res>;
  using stored_cnt_res = std::conditional_t<std::is_void<cnt_res>::value, void_val, cnt_res>;

  cnt_data(F func, std::shared_ptr<future_state<T>> parent):
    storage_(first_t{}, std::move(func), std::move(parent))
  {}

  continuations_stack& continuations() override {return  continuations_;}

  std::add_lvalue_reference_t<state_storage_t<res_t>> value_ref() override {
    if (exception_)
      std::rethrow_exception(exception_);
    return unwrapped_ref(storage_.get(second_t{}));
  }

  std::exception_ptr exception() override {return exception_;}

  void abandon() {
    if (continuations_.executed())
      return;
    exception_ = std::make_exception_ptr(std::future_error{std::future_errc::broken_promise});
    continuations_.execute();
  }

  either<cnt_closure<F, T>, stored_cnt_res> storage_;
  std::exception_ptr exception_ = nullptr;
  continuations_stack continuations_;
};

template<cnt_tag Tag, typename T, typename F>
auto then_unwrap(std::shared_ptr<cnt_data<Tag, T, F>> data) -> std::enable_if_t<
  !is_future<cnt_result_t<F, cnt_arg_t<Tag, T>>>::value
> {
  data->continuations_.execute();
}

// implicit unwrap
template<cnt_tag Tag, typename T, typename F>
auto then_unwrap(std::shared_ptr<cnt_data<Tag, T, F>> data) -> std::enable_if_t<
  is_future<cnt_result_t<F, cnt_arg_t<Tag, T>>>::value
> {
  auto& res_future = data->storage_.get(second_t{});
  if (!res_future.valid()) {
    data->exception_ = std::make_exception_ptr(std::future_error{std::future_errc::broken_promise});
    data->continuations_.execute();
    return;
  }
  auto& continuations = state_of(res_future)->continuations();
  continuations.push([wdata = weak(std::move(data))] {
    if (auto data = wdata.lock())
      data->continuations_.execute();
  });
}

template<cnt_tag Tag, typename S, typename F, typename T>
auto invoke_emplace(S& storage, F&& func, std::shared_ptr<future_state<T>>) -> std::enable_if_t<
  std::is_void<cnt_arg_t<Tag, T>>::value && !std::is_void<
    decltype(portable_concurrency::detail::invoke(std::forward<F>(func)))
  >::value
> {
  storage.emplace(
    second_t{},
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
  storage.emplace(second_t{});
}

template<cnt_tag Tag, typename S, typename F, typename T>
auto invoke_emplace(S& storage, F&& func, std::shared_ptr<future_state<T>> parent) -> std::enable_if_t<
  !std::is_void<cnt_arg_t<Tag, T>>::value && !std::is_void<
    decltype(portable_concurrency::detail::invoke(std::forward<F>(func), cnt_arg<Tag, T>::extract(parent)))
  >::value
> {
  storage.emplace(
    second_t{},
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
  storage.emplace(second_t{});
}

// future::then, shared_future::then
template<cnt_tag Tag, typename T, typename F>
auto then_run(std::shared_ptr<cnt_data<Tag, T, F>> data) try {
  auto& action = data->storage_.get(first_t{});
  if (Tag != cnt_tag::then && Tag != cnt_tag::shared_then) {
    if (auto error = action.parent->exception()) {
      data->storage_.clean();
      data->exception_ = std::move(error);
      data->continuations().execute();
      return;
    }
  }

  invoke_emplace<Tag>(data->storage_, std::move(action.func), std::move(action.parent));
  then_unwrap(data);
} catch (...) {
  data->exception_ = std::current_exception();
  data->storage_.clean();
  data->continuations_.execute();
}

template<cnt_tag Tag, typename T, typename F>
struct cnt_action {
  std::weak_ptr<cnt_data<Tag, T, F>> wdata;

  cnt_action(const cnt_action&) = delete;

  cnt_action(cnt_action&&) noexcept = default;
  cnt_action& operator=(cnt_action&&) noexcept = default;

  void operator() () {
    if (auto data = wdata.lock())
      then_run(std::move(data));
    wdata.reset();
  }

  ~cnt_action() {
    if (auto data = wdata.lock())
      data->abandon();
  }
};

template<cnt_tag Tag, typename T, typename F>
auto make_then_state(std::shared_ptr<future_state<T>> parent, F&& f) {
  using cnt_data_t = cnt_data<Tag, T, std::decay_t<F>>;
  using cnt_action_t = cnt_action<Tag, T, std::decay_t<F>>;
  using res_t = typename cnt_data_t::res_t;

  auto& parent_continuations = parent->continuations();
  auto data = std::make_shared<cnt_data_t>(std::forward<F>(f), std::move(parent));
  parent_continuations.push(cnt_action_t{data});
  return std::shared_ptr<future_state<res_t>>{std::move(data)};
}

template<cnt_tag Tag, typename T, typename E, typename F>
auto make_then_state(std::shared_ptr<future_state<T>> parent, E&& exec, F&& f) {
  using cnt_data_t = cnt_data<Tag, T, std::decay_t<F>>;
  using cnt_action_t = cnt_action<Tag, T, std::decay_t<F>>;
  using res_t = typename cnt_data_t::res_t;
  struct executor_bind {
    std::decay_t<E> exec;
    cnt_data_t data;

    executor_bind(E&& e, F&& f, std::shared_ptr<future_state<T>> parent):
      exec(std::forward<E>(e)),
      data(std::forward<F>(f), std::move(parent))
    {}
  };

  auto& parent_continuations = parent->continuations();
  auto data = std::make_shared<executor_bind>(std::forward<E>(exec), std::forward<F>(f), std::move(parent));
  parent_continuations.push([wdata = weak(data)]() mutable {
    if (auto data = wdata.lock())
      post(std::move(data->exec), cnt_action_t{std::shared_ptr<cnt_data_t>{data, &data->data}});
  });
  return std::shared_ptr<future_state<res_t>>{data, &data->data};
}

} // namespace detail
} // inline namespace cxx14_v1
} // namespace portable_concurrency
