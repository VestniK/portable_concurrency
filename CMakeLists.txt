cmake_minimum_required(VERSION 3.3)
project(portable_concurrency VERSION 0.7.0 LANGUAGES C CXX)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(GNUInstallDirs)
include(BuildTypes)
include(ConanDeps)

set(CMAKE_CXX_STANDARD 14)
if (MSVC)
  add_definitions(/bigobj)
else()
  add_definitions(-Werror=all)
endif()

add_subdirectory(portable_concurrency)

option(BUILD_TESTS "Build library unit tests" On)
if (BUILD_TESTS)
  enable_testing()
  add_subdirectory(test)
endif()
add_subdirectory(examples EXCLUDE_FROM_ALL)

install(FILES portable_concurrency-config.cmake
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/portable_concurrency
  COMPONENT Devel
)
install(
  EXPORT portable_concurrency
  NAMESPACE portable_concurrency::
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/portable_concurrency
)
export(EXPORT portable_concurrency
  NAMESPACE portable_concurrency::
  FILE ${CMAKE_CURRENT_BINARY_DIR}/portable_concurrency.cmake
)
configure_file(
  portable_concurrency-config.cmake
  ${CMAKE_CURRENT_BINARY_DIR}/portable_concurrency-config.cmake
  COPYONLY
)
set(CPACK_GENERATOR TGZ)
set(CPACK_PACKAGE_VERSION ${portable_concurrency_VERSION})
include(CPack)

