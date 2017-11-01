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
template<cnt_tag Tag>
using cnt_tag_t = std::integral_constant<cnt_tag, Tag>;

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

template<typename T, typename R, typename F, typename E>
struct cnt_data;

template<typename T, typename R, typename F>
struct cnt_data<T, R, F, void> {
  F func;
  std::shared_ptr<future_state<T>> parent;
  shared_state<R> state;

  cnt_data(F func, std::shared_ptr<future_state<T>> parent):
    func(std::move(func)), parent(std::move(parent))
  {}
};

template<typename T, typename R, typename F, typename E>
struct cnt_data: cnt_data<T, R, F, void> {
  E exec;

  cnt_data(E exec, F func, std::shared_ptr<future_state<T>> parent):
    cnt_data<T, R, F, void>(std::move(func), std::move(parent)),
    exec(std::move(exec))
  {}
};

template<typename T, typename R, typename F, typename E>
struct cnt_data_holder {
  std::weak_ptr<cnt_data<T, R, F, E>> wdata;

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
template<cnt_tag Tag, typename T, typename R, typename F>
auto then_run(std::shared_ptr<cnt_data<T, R, F, void>> data, cnt_tag_t<Tag>) -> std::enable_if_t<
  is_future<std::result_of_t<F(cnt_arg_t<Tag, T>)>>::value
> {
  using res_t = std::result_of_t<F(cnt_arg_t<Tag, T>)>;
  res_t res;
  try {
    res = ::portable_concurrency::cxx14_v1::detail::invoke(std::move(data->func), std::move(data->parent));
  } catch(...) {
    data->state.set_exception(std::current_exception());
    return;
  }
  if (!res.valid()) {
    data->state.set_exception(std::make_exception_ptr(std::future_error{std::future_errc::broken_promise}));
    return;
  }
  auto& continuations = state_of(res)->continuations();
  continuations.push([data = std::move(data), res = std::move(res)] () mutable {
    set_state_value(data->state, &res_t::get, std::move(res));
  });
}

// simple then
template<cnt_tag Tag, typename T, typename R, typename F>
auto then_run(std::shared_ptr<cnt_data<T, R, F, void>> data, cnt_tag_t<Tag>) -> std::enable_if_t<
  !is_unique_future<std::result_of_t<F(cnt_arg_t<Tag, T>)>>::value
> {
  set_state_value(data->state, std::move(data->func), cnt_arg_t<Tag, T>{std::move(data->parent)});
}

// non-void future::next
template<typename T, typename R, typename F>
void then_run(std::shared_ptr<cnt_data<T, R, F, void>> data, cnt_tag_t<cnt_tag::next>) {
  if (auto error = data->parent->exception()) {
    data->state.set_exception(std::move(error));
    return;
  }
  set_state_value(data->state, std::move(data->func), std::move(data->parent->value_ref()));
}

// void future::next
template<typename R, typename F>
void then_run(std::shared_ptr<cnt_data<void, R, F, void>> data, cnt_tag_t<cnt_tag::next>) {
  if (auto error = data->parent->exception()) {
    data->state.set_exception(std::move(error));
    return;
  }
  set_state_value(data->state, std::move(data->func));
}

// continuation with executor
template<cnt_tag Tag, typename T, typename R, typename F, typename E>
void then_post(std::shared_ptr<cnt_data<T, R, F, E>> data) {
  post(std::move(data->exec), [data_holder = cnt_data_holder<T, R, F, E>{data}]() mutable {
    if (std::shared_ptr<cnt_data<T, R, F, void>> data = data_holder.wdata.lock())
      then_run(std::move(data), cnt_tag_t<Tag>{});
  });
}

template<cnt_tag Tag, typename T, typename F>
auto make_then_state(std::shared_ptr<future_state<T>> parent, F&& f) {
  using CntRes = cnt_result_t<F, cnt_arg_t<Tag, T>>;
  using R = remove_future_t<CntRes>;

  auto data = std::make_shared<cnt_data<T, R, std::decay_t<F>, void>>(
    std::forward<F>(f), std::move(parent)
  );
  data->parent->continuations().push([wdata = weak(data)]() mutable {
    if (auto data = wdata.lock())
      then_run(std::move(data), cnt_tag_t<Tag>{});
  });
  return std::shared_ptr<future_state<R>>{data, &data->state};
}

template<cnt_tag Type, typename T, typename E, typename F>
auto make_then_state(std::shared_ptr<future_state<T>> parent, E&& exec, F&& f) {
  using CntRes = cnt_result_t<F, cnt_arg_t<Type, T>>;
  using R = remove_future_t<CntRes>;

  auto data = std::make_shared<cnt_data<T, R, std::decay_t<F>, std::decay_t<E>>>(
    std::forward<E>(exec), std::forward<F>(f), std::move(parent)
  );
  data->parent->continuations().push([wdata = weak(data)]() mutable {
    if (auto data = wdata.lock())
      then_post<Type>(std::move(data));
  });
  return std::shared_ptr<future_state<R>>{data, &data->state};
}

} // namespace detail
} // inline namespace cxx14_v1
} // namespace portable_concurrency
