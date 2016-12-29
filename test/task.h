#pragma once

#include <functional>
#include <memory>

class task {
public:
  task() = default;

  // TODO: task([](my_type&& var) {...}, std::move(my_type_var)); <--- do not compiles
  // WORKAROUND: task([](my_type& var) {...}, std::move(my_type_var)); <--- acts mostly the same way
  template<typename F, typename... A>
  task(F&& f, A&&... a):
    type_erasure_(make_type_erasure(std::bind(std::forward<F>(f), std::forward<A>(a)...)))
  {}

  void operator() () {
    if (!type_erasure_)
      throw std::bad_function_call();
    type_erasure_->run();
    type_erasure_.reset();
  }

private:
  struct type_erasure_base {
    virtual ~type_erasure_base() = default;
    virtual void run() = 0;
  };

  template<typename F>
  struct type_erasure: type_erasure_base {
    type_erasure(F&& f): f(std::forward<F>(f)) {}

    void run() override {f();}

    std::decay_t<F> f;
  };

  template<typename F>
  type_erasure_base* make_type_erasure(F&& f) {
    return new type_erasure<F>(std::forward<F>(f));
  }

  std::unique_ptr<type_erasure_base> type_erasure_;
};