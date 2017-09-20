project(portable_concurrency)
cmake_minimum_required(VERSION 3.0)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

option(BUILD_SHARED_LIBS "Specify to build shared or static library." Off)
option(ENABLE_UBSAN "Enables undefined behaviour sanitizer for clang and gcc" Off)
option(ENABLE_ADDRSAN "Enables address sanitizer for clang and gcc" Off)
option(ENABLE_THREADSAN "Enables thread sanitizer for clang and gcc" Off)

include(HeaderOnlyTools)
include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
set(CONAN_CMAKE_FIND_ROOT_PATH ${CONAN_CMAKE_MODULE_PATH})
conan_basic_setup(TARGETS)

set(CMAKE_CXX_STANDARD 14)
if (MSVC)
  add_definitions(/bigobj)
else()
  add_definitions(-Werror=all)
endif()

add_subdirectory(portable_concurrency)

enable_testing()
add_subdirectory(test)

file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/portable_concurrency-config.cmake
  "include(\"\${CMAKE_CURRENT_LIST_DIR}/portable_concurrency.cmake\")"
)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/portable_concurrency-config.cmake
  DESTINATION lib/cmake/portable_concurrency
  COMPONENT Devel
)
install(EXPORT portable_concurrency NAMESPACE portable_concurrency:: DESTINATION lib/cmake/portable_concurrency)
