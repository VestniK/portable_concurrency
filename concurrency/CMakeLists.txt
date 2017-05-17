find_package(Threads REQUIRED)

add_subdirectory(bits)

set(HEADERS
  future
  latch
)

add_library(concurrency INTERFACE)
target_link_libraries(concurrency INTERFACE Threads::Threads)
target_include_directories(concurrency INTERFACE
  $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
  $<INSTALL_INTERFACE:include>
)

install(TARGETS concurrency EXPORT concurrency
  RUNTIME DESTINATION bin
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
)
install(FILES ${HEADERS}
  DESTINATION include/concurrency COMPONENT Devel
)
