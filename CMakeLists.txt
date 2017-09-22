project(portable_concurrency)
cmake_minimum_required(VERSION 3.0)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(HeaderOnlyTools)
include(BuildTypes)
include(ConanDeps)

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
