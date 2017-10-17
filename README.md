# portable_concurrency

[![CC0 licensed](http://i.creativecommons.org/p/zero/1.0/88x31.png)](https://creativecommons.org/publicdomain/zero/1.0/)
[![Build Status](https://travis-ci.org/VestniK/portable_concurrency.svg?branch=master)](https://travis-ci.org/VestniK/portable_concurrency)
[![Build status](https://ci.appveyor.com/api/projects/status/r2d3py3ioae5bv7u?svg=true)](https://ci.appveyor.com/project/VestniK/portable-concurrency)

Attempt to write portable implementation of the C++ Extensions for Concurrency, ISO/IEC TS 19571:2016

 * All of the classes required by TS namespace *std::experimental::concurrency_v1* resides in the namespace *portable_concurrency::cxx14_v1*.
 * The only public dependency: standard library.
 * The only build dependency: gtest

## Build

    mkdir -p build/debug
    cd build/debug
    conan remote add VestniK https://api.bintray.com/conan/vestnik/VestniK
    conan install --build=missing ../..
    cmake -G Ninja -DCMAKE_BUILD_TYPE=Debug ../..
    ninja
    ninja test

## TODO

 * make_ready_at_thread_exit for promise and packaged_task
 * allocator support where required by the standard
