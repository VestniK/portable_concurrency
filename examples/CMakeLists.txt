# Ugly hack to make find_packages(portable_concurrency) work in examples
configure_file(
  portable_concurrency.cmake
  ${CMAKE_CURRENT_BINARY_DIR}/portable_concurrency/portable_concurrency.cmake
  COPYONLY
)
configure_file(
  ${portable_concurrency_SOURCE_DIR}/portable_concurrency-config.cmake
  ${CMAKE_CURRENT_BINARY_DIR}/portable_concurrency/portable_concurrency-config.cmake
  COPYONLY
)
list(APPEND CMAKE_PREFIX_PATH "${CMAKE_CURRENT_BINARY_DIR}")

find_package(CoroutinesTS QUIET)
if (CoroutinesTS_FOUND)
  add_subdirectory(coroutine)
endif()

find_package(Qt5Core QUIET)
if (Qt5Core_FOUND)
  add_subdirectory(qt)
endif()

find_package(asio QUIET)
if (asio_FOUND)
  add_subdirectory(asio)
endif()
