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

 * future<future<T>>, future<shared_future<T>>, shared_future<future<T>> and shared_future<shared_future<T>> are not
   allowed.
 * No packaged_task<R(A...)>::reset() method provided.
 * Basic executor support for future::then and shared_future::then

## Build

    mkdir -p build/debug
    cd build/debug
    conan remote add VestniK https://api.bintray.com/conan/vestnik/VestniK
    conan install --build=missing ../..
    cmake -G Ninja -DCMAKE_BUILD_TYPE=Debug ../..
    ninja
    ninja test
