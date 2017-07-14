#!/bin/bash -e

SRCDIR=$(dirname $(dirname $0))

export CONAN_ENV_COMPILER_LIBCXX=$1
export BUILD_TYPE=$2

export CONAN_CMAKE_GENERATOR=Ninja

conan install -s build_type=${BUILD_TYPE} --build=missing ${SRCDIR}

cmake -G Ninja -DCMAKE_BUILD_TYPE=${BUILD_TYPE} -DCONAN_LIBCXX=${CONAN_ENV_COMPILER_LIBCXX} ${SRCDIR}
ninja

./bin/unit_tests
