project(concurrency)
cmake_minimum_required(VERSION 3.0)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

option(BUILD_SHARED_LIBS "Specify to build shared or static library." Off)
option(ENABLE_UBSAN "Enables undefined behaviour sanitizer for clang and gcc" Off)

include(conanbuildinfo.cmake)
conan_basic_setup()

set(CMAKE_CXX_STANDARD 14)
if (NOT ${CMAKE_CXX_COMPILER_ID} STREQUAL "MSVC")
  add_definitions(-Werror=all)
endif()

add_subdirectory(concurrency)

enable_testing()
add_subdirectory(test)

file(WRITE ${CMAKE_BINARY_DIR}/concurrency-config.cmake
  "include(\"\${CMAKE_CURRENT_LIST_DIR}/concurrency.cmake\")"
)
install(FILES ${CMAKE_BINARY_DIR}/concurrency-config.cmake
  DESTINATION lib/cmake/concurrency
  COMPONENT Devel
)
install(EXPORT concurrency NAMESPACE concurrency:: DESTINATION lib/cmake/concurrency)
