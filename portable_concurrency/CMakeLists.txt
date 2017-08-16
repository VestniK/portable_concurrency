 find_package(Threads REQUIRED)


set(HEADERS
  functional
  future
  latch
)

add_library(portable_concurrency INTERFACE)
target_link_libraries(portable_concurrency INTERFACE Threads::Threads)
target_include_directories(portable_concurrency INTERFACE
  $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
  $<INSTALL_INTERFACE:include>
)
headeronly_target_headers(portable_concurrency ${HEADERS})

add_subdirectory(bits)

install(TARGETS portable_concurrency EXPORT portable_concurrency
  RUNTIME DESTINATION bin
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
)
install(FILES ${HEADERS}
  DESTINATION include/portable_concurrency COMPONENT Devel
)
