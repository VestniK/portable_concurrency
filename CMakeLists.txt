cmake_minimum_required(VERSION 3.15 FATAL_ERROR)
project(portable_concurrency
  VERSION 0.11.0
  DESCRIPTION "Lightweight future/promise C++ library"
)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(GNUInstallDirs)
include(BuildTypes)

option(PC_NO_DEPRECATED "Remove deprecated API from the build and installation." OFF)
option(PC_DEV_BUILD "Build mode for library developers. Important warnings are treated as errors." OFF)
if (PC_DEV_BUILD AND NOT MSVC)
  add_compile_options(-Werror=all)
endif()

add_subdirectory(portable_concurrency)

option(PC_BUILD_TESTS "Build library unit tests" ON)
if (PC_BUILD_TESTS)
  enable_testing()
  add_subdirectory(test)
endif()

option(PC_BUILD_EXAMPLES "Build examples" OFF)
if (PC_BUILD_EXAMPLES)
  add_subdirectory(examples)
endif()

# Installation
include(CMakePackageConfigHelpers)
write_basic_package_version_file(
  "${CMAKE_CURRENT_BINARY_DIR}/portable_concurrency-config-version.cmake"
  VERSION ${${PROJECT_NAME}_VERSION}
  COMPATIBILITY SameMajorVersion
)
install(FILES portable_concurrency-config.cmake "${CMAKE_CURRENT_BINARY_DIR}/portable_concurrency-config-version.cmake"
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/portable_concurrency
)
install(
  EXPORT portable_concurrency
  NAMESPACE portable_concurrency::
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/portable_concurrency
)
# Build tree export
export(EXPORT portable_concurrency
  NAMESPACE portable_concurrency::
  FILE ${CMAKE_CURRENT_BINARY_DIR}/portable_concurrency.cmake
)
configure_file(
  portable_concurrency-config.cmake
  ${CMAKE_CURRENT_BINARY_DIR}/portable_concurrency-config.cmake
  COPYONLY
)
# Package generation
set(CPACK_GENERATOR TGZ)
set(CPACK_PACKAGE_VERSION ${portable_concurrency_VERSION})
include(CPack)

