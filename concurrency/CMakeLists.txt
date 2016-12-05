find_package(Threads REQUIRED)

add_subdirectory(bits)

set(HEADERS
  future
)

add_library(concurrency concurrency.cpp)
target_link_libraries(concurrency Threads::Threads)
target_include_directories(concurrency PUBLIC
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
