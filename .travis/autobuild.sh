#!/bin/sh

set -e

export CC=$1
export CXX=$2
export CONAN_ENV_COMPILER_LIBCXX=$3
export BUILD_TYPE=$4

export CONAN_CMAKE_GENERATOR=Ninja

conan install -s build_type=${BUILD_TYPE} --build=missing

mkdir build
cd build
cmake -G Ninja -DCMAKE_BUILD_TYPE=${BUILD_TYPE} -DCONAN_LIBCXX=${CONAN_ENV_COMPILER_LIBCXX} ..
ninja
./bin/unit_tests
