project(portable_concurrency)
cmake_minimum_required(VERSION 3.3)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
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

install(FILES portable_concurrency-config.cmake
  DESTINATION lib/cmake/portable_concurrency
  COMPONENT Devel
)
install(EXPORT portable_concurrency NAMESPACE portable_concurrency:: DESTINATION lib/cmake/portable_concurrency)
export(EXPORT portable_concurrency
  NAMESPACE portable_concurrency::
  FILE ${CMAKE_CURRENT_BINARY_DIR}/portable_concurrency/portable_concurrency.cmake
)
configure_file(
  portable_concurrency-config.cmake
  ${CMAKE_CURRENT_BINARY_DIR}/portable_concurrency/portable_concurrency-config.cmake
  COPYONLY
)
set(CPACK_GENERATOR TGZ)
include(CPack)

add_subdirectory(examples EXCLUDE_FROM_ALL)
