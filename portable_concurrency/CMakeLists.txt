 find_package(Threads REQUIRED)

add_subdirectory(bits)

set(HEADERS
  future
  latch
)

add_library(portable_concurrency INTERFACE)
target_link_libraries(portable_concurrency INTERFACE Threads::Threads)
target_include_directories(portable_concurrency INTERFACE
  $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
  $<INSTALL_INTERFACE:include>
)

install(TARGETS portable_concurrency EXPORT portable_concurrency
  RUNTIME DESTINATION bin
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
)
install(FILES ${HEADERS}
  DESTINATION include/portable_concurrency COMPONENT Devel
)
