#!/bin/bash -e

SRCDIR=$(dirname $(dirname $0))

export BUILD_TYPE=$2

export CONAN_CMAKE_GENERATOR=Ninja

conan config set settings_defaults.compiler.libcxx=$1 &>/dev/null
conan config get settings_defaults

conan install -s build_type=${BUILD_TYPE} --build=missing ${SRCDIR}

cmake -G Ninja ${SRCDIR} \
  -DCMAKE_BUILD_TYPE=${BUILD_TYPE} \
  -DCONAN_LIBCXX=$(conan config get settings_defaults.compiler.libcxx) \
  -DCONAN_COMPILER=$(conan config get settings_defaults.compiler) \
  -DCONAN_COMPILER_VERSION=$(conan config get settings_defaults.compiler.version)
ninja

./bin/unit_tests
