option(CONAN "Use dependencies provided by conan package manager" Off)

if (CONAN)
  include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
  set(CONAN_CMAKE_FIND_ROOT_PATH ${CONAN_CMAKE_MODULE_PATH})
  conan_basic_setup(TARGETS)
endif()
