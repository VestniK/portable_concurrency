# portable_concurrency

[![Build Status](https://travis-ci.org/VestniK/portable_concurency.svg?branch=master)](https://travis-ci.org/VestniK/portable_concurency)

Attempt to write portable implementation of the C++ Extensions for Concurrency, ISO/IEC TS 19571:2016

 * All of the classes required by TS namespace *std::experimental::concurrency_v1* resides in the namespace *experimental::concurrency_v1*.
 * The only public dependency: standard library.
 * The only build dependency: gtest

## Build

    conan install --build=missing
    mkdir -p build/debug
    cd build/debug
    cmake -G Ninja -DCMAKE_BUILD_TYPE=Debug ../..
    ninja
    ninja test
