#!/bin/bash -e

SRCDIR=$(dirname $(dirname $0))

export LIBCXX=$1
export BUILD_TYPE=$2

# TODO: implement sanitizeds support as custom cmake build types
if [ "${BUILD_TYPE}" == "UBSAN" ] || [ "${BUILD_TYPE}" == "ADDRSAN" ] || [ "${BUILD_TYPE}" == "THREADSAN" ]
then
  export SNITIZER_CMAKE_OPTIONS="-DENABLE_${BUILD_TYPE}=On"
  export BUILD_TYPE=Debug
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

conan install -s build_type=${BUILD_TYPE} --build=missing ${SRCDIR}

cmake -G Ninja ${SRCDIR} \
  -DCONAN=On \
  -DCMAKE_BUILD_TYPE=${BUILD_TYPE} \
  -DCONAN_LIBCXX=$(conan_profile_get compiler.libcxx default) \
  -DCONAN_COMPILER=$(conan_profile_get compiler default) \
  -DCONAN_COMPILER_VERSION=$(conan_profile_get compiler.version default) \
  ${SNITIZER_CMAKE_OPTIONS}
ninja

./bin/unit_tests
