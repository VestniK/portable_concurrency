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
struct cnt_action {
  F func;
  std::shared_ptr<future_state<T>> parent;
};

template<cnt_tag Tag, typename T, typename F>
struct cnt_data {
  using cnt_res = cnt_result_t<F, cnt_arg_t<Tag, T>>;
  using res_t = remove_future_t<cnt_res>;

  cnt_action<F, T> action;
  shared_state<res_t> state;

  cnt_data(F func, std::shared_ptr<future_state<T>> parent):
    action({std::move(func), std::move(parent)})
  {}
};

template<cnt_tag Tag, typename T, typename F>
struct cnt_data_holder {
  std::weak_ptr<cnt_data<Tag, T, F>> wdata;

  cnt_data_holder(const cnt_data_holder&) = delete;

  cnt_data_holder(cnt_data_holder&&) noexcept = default;
  cnt_data_holder& operator=(cnt_data_holder&&) noexcept = default;

  ~cnt_data_holder() {
    auto data = wdata.lock();
    if (data && !data->state.continuations().executed())
      data->state.set_exception(std::make_exception_ptr(std::future_error{std::future_errc::broken_promise}));
  }
};

// implicit unwrap
template<cnt_tag Tag, typename T, typename F>
auto then_run(std::shared_ptr<cnt_data<Tag, T, F>> data) -> std::enable_if_t<
  is_future<std::result_of_t<F(cnt_arg_t<Tag, T>)>>::value
> {
  using res_t = std::result_of_t<F(cnt_arg_t<Tag, T>)>;
  res_t res;
  try {
    res = ::portable_concurrency::cxx14_v1::detail::invoke(
      std::move(data->action.func),
      cnt_arg_t<Tag, T>{std::move(data->action.parent)}
    );
  } catch(...) {
    data->state.set_exception(std::current_exception());
    return;
  }
  if (!res.valid()) {
    data->state.set_exception(std::make_exception_ptr(std::future_error{std::future_errc::broken_promise}));
    return;
  }
  auto& continuations = state_of(res)->continuations();
  continuations.push([wdata = weak(std::move(data)), res = std::move(res)] () mutable {
    if (auto data = wdata.lock())
      set_state_value(data->state, &res_t::get, std::move(res));
  });
}

// simple then
template<cnt_tag Tag, typename T, typename F>
auto then_run(std::shared_ptr<cnt_data<Tag, T, F>> data) -> std::enable_if_t<
  !is_unique_future<std::result_of_t<F(cnt_arg_t<Tag, T>)>>::value
> {
  set_state_value(
    data->state,
    std::move(data->action.func),
    cnt_arg_t<Tag, T>{std::move(data->action.parent)});
}

// non-void future::next
template<typename T, typename F>
void then_run(std::shared_ptr<cnt_data<cnt_tag::next, T, F>> data) {
  if (auto error = data->action.parent->exception()) {
    data->state.set_exception(std::move(error));
    return;
  }
  set_state_value(data->state, std::move(data->action.func), std::move(data->action.parent->value_ref()));
}

// void future::next
template<typename F>
void then_run(std::shared_ptr<cnt_data<cnt_tag::next, void, F>> data) {
  if (auto error = data->action.parent->exception()) {
    data->state.set_exception(std::move(error));
    return;
  }
  set_state_value(data->state, std::move(data->action.func));
}

template<cnt_tag Tag, typename T, typename F>
auto make_then_state(std::shared_ptr<future_state<T>> parent, F&& f) {
  using cnt_data_t = cnt_data<Tag, T, std::decay_t<F>>;
  using res_t = typename cnt_data_t::res_t;

  auto data = std::make_shared<cnt_data_t>(
    std::forward<F>(f), std::move(parent)
  );
  data->action.parent->continuations().push([wdata = weak(data)]() mutable {
    if (auto data = wdata.lock())
      then_run(std::move(data));
  });
  return std::shared_ptr<future_state<res_t>>{data, &data->state};
}

template<cnt_tag Tag, typename T, typename E, typename F>
auto make_then_state(std::shared_ptr<future_state<T>> parent, E&& exec, F&& f) {
  using cnt_data_t = cnt_data<Tag, T, std::decay_t<F>>;
  using res_t = typename cnt_data_t::res_t;

  auto data = std::make_shared<cnt_data_t>(
    std::forward<F>(f), std::move(parent)
  );
  data->action.parent->continuations().push([exec = std::decay_t<E>{std::forward<E>(exec)}, wdata = weak(data)]() mutable {
    if (auto data = wdata.lock()) {
      post(std::move(exec), [holder = cnt_data_holder<Tag, T, std::decay_t<F>>{std::move(wdata)}] () mutable {
        if (auto data = holder.wdata.lock())
          then_run(std::move(data));
      });
    }
  });
  return std::shared_ptr<future_state<res_t>>{data, &data->state};
}

} // namespace detail
} // inline namespace cxx14_v1
} // namespace portable_concurrency
