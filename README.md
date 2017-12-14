# portable_concurrency

[![Build Status](https://travis-ci.org/VestniK/portable_concurrency.svg?branch=master)](https://travis-ci.org/VestniK/portable_concurrency)
[![Build status](https://ci.appveyor.com/api/projects/status/r2d3py3ioae5bv7u?svg=true)](https://ci.appveyor.com/project/VestniK/portable-concurrency)

[![CC0 licensed](http://i.creativecommons.org/p/zero/1.0/88x31.png)](https://creativecommons.org/publicdomain/zero/1.0/)

Future API implementation from the "C++ Extensions for Concurrency, ISO/IEC TS 19571:2016" with some experimental
extensions. Strict TS implementation is developed in the `strict-ts` branch.

 * All of the classes required by TS namespace *std::experimental::concurrency_v1* resides in the namespace *portable_concurrency::cxx14_v1*.
 * The only public dependency: standard library.
 * The only build dependency: gtest

## Difference from TS

 * `future<future<T>>`, `future<shared_future<T>>`, `shared_future<future<T>>` and `shared_future<shared_future<T>>` are
   not allowed.
 * Implicit unwrap for continuations with `shared_future` result type.
 * No `packaged_task<R(A...)>::reset()` method provided.
 * `future::next` and `shared_future::next` implementation from N3865.
 * Basic executor support for all fucntions attaching continuations.
 * Executor aware version of async.
 * `future` destuctor (as well as destructor of the last `shared_future` pointing to particular shared state) has cancel
   semantics. If continuation or `packaged_task` which calculates value fot this `future` is not yet started it will not
   be executed at all.

## Build

    mkdir -p build/debug
    cd build/debug
    conan remote add pdeps https://api.bintray.com/conan/pdeps/deps
    conan install --build=missing ../..
    cmake -G Ninja -DCMAKE_BUILD_TYPE=Debug ../..
    ninja
    ninja test
