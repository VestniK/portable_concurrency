#!/bin/bash -e

SRCDIR=$(dirname $(dirname $0))

export LIBCXX=$1
export BUILD_TYPE=$2
export DEPS_BUILD_TYPE=${BUILD_TYPE}

export LSAN_OPTIONS=verbosity=1:log_threads=1

if [ "${BUILD_TYPE}" == "UBsan" ] || [ "${BUILD_TYPE}" == "Asan" ] || [ "${BUILD_TYPE}" == "Tsan" ]
then
  export DEPS_BUILD_TYPE=Debug
fi

# https://github.com/conan-io/conan/issues/1723
conan_profile_get() {
  KEY=$1
  PROFILE=$2

  conan profile show ${PROFILE} | grep "${KEY}:" | grep -o -E "[^[:space:]]+$"
}

export CONAN_CMAKE_GENERATOR=Ninja

conan --version
conan profile update settings.compiler.libcxx="${LIBCXX}" default
# dump toolchain info
conan profile show default

conan install -s build_type=${DEPS_BUILD_TYPE} --build=missing ${SRCDIR}

cmake -G Ninja ${SRCDIR} \
  -DCONAN=On \
  -DCMAKE_BUILD_TYPE=${BUILD_TYPE} \
  -DCONAN_LIBCXX=$(conan_profile_get compiler.libcxx default) \
  -DCONAN_COMPILER=$(conan_profile_get compiler default) \
  -DCONAN_COMPILER_VERSION=$(conan_profile_get compiler.version default)
ninja

./bin/unit_tests
