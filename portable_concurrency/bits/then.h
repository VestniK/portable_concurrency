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
struct cnt_arg<cnt_tag::then, T> {using type = future<T>;};
template<typename T>
struct cnt_arg<cnt_tag::next, T> {using type = T;};
template<typename T>
struct cnt_arg<cnt_tag::shared_then, T> {using type = shared_future<T>;};

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
  !is_future<std::result_of_t<F(cnt_arg_t<Tag, T>)>>::value
> {
  data->continuations_.execute();
}

// implicit unwrap
template<cnt_tag Tag, typename T, typename F>
auto then_unwrap(std::shared_ptr<cnt_data<Tag, T, F>> data) -> std::enable_if_t<
  is_future<std::result_of_t<F(cnt_arg_t<Tag, T>)>>::value
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

template<typename S, typename F, typename... A>
auto invoke_emplace(S& storage, F&& func, A&&... args) -> std::enable_if_t<
  !std::is_void<decltype(portable_concurrency::detail::invoke(std::forward<F>(func), std::forward<A>(args)...))>::value
> {
  storage.emplace(second_t{}, portable_concurrency::detail::invoke(std::forward<F>(func), std::forward<A>(args)...));
}

template<typename S, typename F, typename... A>
auto invoke_emplace(S& storage, F&& func, A&&... args) -> std::enable_if_t<
  std::is_void<decltype(portable_concurrency::detail::invoke(std::forward<F>(func), std::forward<A>(args)...))>::value
> {
  portable_concurrency::detail::invoke(std::forward<F>(func), std::forward<A>(args)...);
  storage.emplace(second_t{});
}

// future::then, shared_future::then
template<cnt_tag Tag, typename T, typename F>
auto then_run(std::shared_ptr<cnt_data<Tag, T, F>> data) {
  auto& action = data->storage_.get(first_t{});
  try {
    invoke_emplace(data->storage_, std::move(action.func), cnt_arg_t<Tag, T>{std::move(action.parent)});
    then_unwrap(data);
  } catch (...) {
    data->exception_ = std::current_exception();
    data->storage_.clean();
    data->continuations_.execute();
  }
}

// non-void future::next
template<typename T, typename F>
void then_run(std::shared_ptr<cnt_data<cnt_tag::next, T, F>> data) {
  auto& action = data->storage_.get(first_t{});
  if (auto error = action.parent->exception()) {
    data->storage_.clean();
    data->exception_ = std::move(error);
    data->continuations().execute();
    return;
  }

  try {
    invoke_emplace(data->storage_, std::move(action.func), std::move(action.parent->value_ref()));
  } catch (...) {
    data->exception_ = std::current_exception();
    data->storage_.clean();
  }
  data->continuations_.execute();
}

// void future::next
template<typename F>
void then_run(std::shared_ptr<cnt_data<cnt_tag::next, void, F>> data) {
  auto& action = data->storage_.get(first_t{});
  if (auto error = action.parent->exception()) {
    data->storage_.clean();
    data->exception_ = std::move(error);
    data->continuations().execute();
    return;
  }

  try {
    invoke_emplace(data->storage_, std::move(action.func));
  } catch (...) {
    data->exception_ = std::current_exception();
    data->storage_.clean();
  }
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
  using res_t = typename cnt_data_t::res_t;

  auto& parent_continuations = parent->continuations();
  auto data = std::make_shared<cnt_data_t>(std::forward<F>(f), std::move(parent));
  parent_continuations.push(cnt_action<Tag, T, std::decay_t<F>>{data});
  return std::shared_ptr<future_state<res_t>>{std::move(data)};
}

template<cnt_tag Tag, typename T, typename E, typename F>
auto make_then_state(std::shared_ptr<future_state<T>> parent, E&& exec, F&& f) {
  using cnt_data_t = cnt_data<Tag, T, std::decay_t<F>>;
  using res_t = typename cnt_data_t::res_t;

  auto& parent_continuations = parent->continuations();
  auto data = std::make_shared<cnt_data_t>(std::forward<F>(f), std::move(parent));
  parent_continuations.push([exec = std::decay_t<E>{std::forward<E>(exec)}, wdata = weak(data)]() mutable {
    if (!wdata.expired())
      post(std::move(exec), cnt_action<Tag, T, std::decay_t<F>>{std::move(wdata)});
  });
  return std::shared_ptr<future_state<res_t>>{std::move(data)};
}

} // namespace detail
} // inline namespace cxx14_v1
} // namespace portable_concurrency
