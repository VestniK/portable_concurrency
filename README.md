# portable_concurrency

[![Build Status](https://travis-ci.org/VestniK/portable_concurrency.svg?branch=master)](https://travis-ci.org/VestniK/portable_concurrency)
[![Build status](https://ci.appveyor.com/api/projects/status/r2d3py3ioae5bv7u?svg=true)](https://ci.appveyor.com/project/VestniK/portable-concurrency)

[![CC0 licensed](http://i.creativecommons.org/p/zero/1.0/88x31.png)](https://creativecommons.org/publicdomain/zero/1.0/)

`std::future` done right. Simple to use portable implementation of the future/promise API inspired by the 
[C++ Extensions for Concurrency TS](https://wg21.link/p0159) and some parts of [A Unified Executors Proposal for C++](https://wg21.link/p0443).

 * Minimum requred C++ standard: C++14
 * The only public dependency: standard library.
 * The only build dependency: gtest

## Key features

 * Execcutor aware
   ```cpp
   pc::static_thread_pool pool{5};
   pc::future<int> answer = pc::async(pool.executor(), [] {return 42;});
   ```
 * Designed to work with user provided executors
   ```cpp
   namespace portable_concurrency {
   template<> struct is_executor<QThreadPool*>: std::true_type {};
   }
   void post(QThreadPool*, pc::unique_function<void()>);
   
   pc::future<int> answer = pc::async(QThreadPool::globalInstance(), [] {return 42;});
   ```
 * Continuations support
   ```cpp
   pc::static_thread_pool pool{5};
   asio::io_context io;
   pc::future<rapidjson::Document> res = pc::async(io.get_executor(), receive_document)
     .next(pool.executor(), parse_document);
   ```
 * `when_all`/`when_any` composition of futures
   ```cpp
   pc::future<rapidjson::Document> res = pc::when_any(
     pc::async(pool.executor(), fetch_from_cache),
     pc::async(io.get_executor(), receive_from_network)
   ).next([](auto when_any_res) {
     switch(when_any_res.index) {
       case 0: return std::get<0>(when_any_res.futures);
       case 1: return std::get<1>(when_any_res.futures);
     }
   }); 
   ```
 * `future<future<T>>` transparently unwrapped to `future<T>`
 * `future<shared_future<T>>` transparently unwrapped to `shred_future<T>`
 * Automatic task cancelation:
   * Not yet started functions passed to `pc::async`/`pc::packaged_task` or attached to intermediate futures as continuations
     may not be executed at all if `future` or all `shared_future`'s on the result of continuations chain is destroyed.
   * `future::detach()` and `shared_future::detach()` functions allows to destroy future without cancelation of any tasks.
   ```cpp
   auto future = pc::async(pool.executor(), important_calculation)
     .next(io.get_executor(), important_io)
     .detach()
     .next(pool.executor(), not_so_important_calculations);
   ```
   `important_calculation` and `important_io` are guarantied to be executed even in case of premature future destruction.
   * `promise::is_awaiten()` allows to check if `future` or the last `shared_future` refferensing shared state associated
     with this `promise` is already destroyed.
   * `promise::promise(canceler_arg_t, CancelAction)` constructor allows to specify action which is called in `future`
     or the last `shared_future` refferensing shared state associated with this `promise` is destroyed before value or
     exception was set.
   * Additional `then` overload to check if task is canceled from the continuations function
     ```cpp
     pc::future<std::string> res = pc::async(pool.executor(), [] {return 42;})
       .then([](pc::promise<std::string> p, pc::future<int> f) {
         std::string res;
         while (has_work_to_do()) {
           do_next_step(res, f);
           if (!p.is_awaiten())
             return;
         }
         p.set_value(res);
       });
     ```

## Build

    mkdir -p build/debug
    cd build/debug
    conan remote add pdeps https://api.bintray.com/conan/pdeps/deps
    conan install --build=missing ../..
    cmake -G Ninja -DCMAKE_BUILD_TYPE=Debug ../..
    ninja
    ninja test
