find_package(Threads REQUIRED)

file(WRITE
  ${CMAKE_CURRENT_BINARY_DIR}/check/check.cpp
  "int main() {return 0;} void foo() noexcept(true) {} void bar() noexcept(false) {}"
)
file(WRITE
  ${CMAKE_CURRENT_BINARY_DIR}/check/CMakeLists.txt
  "project(check)
  cmake_minimum_required(VERSION 3.3)
  set(CMAKE_CXX_STANDARD 14)
  add_executable(check check.cpp)"
)
try_compile(
  HAS_CONDITIONAL_NOEXCEPT
  ${CMAKE_CURRENT_BINARY_DIR}/check/build
  ${CMAKE_CURRENT_BINARY_DIR}/check
  check
)
file(WRITE
  ${CMAKE_CURRENT_BINARY_DIR}/check/check.cpp
  "#include <type_traits>
  int main() {return 0;}
  std::aligned_union_t<1, short, double> var;"
)
try_compile(
  HAS_STD_ALIGNED_UNION
  ${CMAKE_CURRENT_BINARY_DIR}/check/build
  ${CMAKE_CURRENT_BINARY_DIR}/check
  check
)
configure_file(bits/config.h.in ${CMAKE_CURRENT_BINARY_DIR}/bits/config.h @ONLY)

set(PUBLIC_HEADERS
  execution
  functional
  functional_fwd
  future
  future_fwd
  latch
)
set(INSTALL_HEADERS
  bits/alias_namespace.h
  bits/align.h
  bits/async.h
  bits/concurrency_type_traits.h
  bits/continuations_stack.h
  bits/either.h
  bits/execution.h
  bits/future.h
  bits/future.hpp
  bits/future_sequence.h
  bits/future_state.h
  bits/fwd.h
  bits/invoke.h
  bits/latch.h
  bits/make_future.h
  bits/once_consumable_stack.h
  bits/packaged_task.h
  bits/promise.h
  bits/shared_future.h
  bits/shared_future.hpp
  bits/shared_state.h
  bits/small_unique_function.h
  bits/small_unique_function.hpp
  bits/then.h
  bits/unique_function.h
  bits/unique_function.hpp
  bits/utils.h
  bits/when_any.h
  bits/when_all.h

  ${CMAKE_CURRENT_BINARY_DIR}/bits/config.h
)

set(HEADERS
  ${PUBLIC_HEADERS}
  ${INSTALL_HEADERS}

  bits/once_consumable_stack.hpp
)

set(SRC
  bits/portable_concurrency.cpp
)

add_library(portable_concurrency ${SRC})
target_sources(portable_concurrency PRIVATE ${HEADERS} bits/config.h.in)
target_link_libraries(portable_concurrency Threads::Threads)
target_include_directories(portable_concurrency PRIVATE ${PROJECT_BINARY_DIR})
target_include_directories(portable_concurrency INTERFACE
  $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
  $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}>
  $<INSTALL_INTERFACE:include>
)

install(TARGETS portable_concurrency EXPORT portable_concurrency
  RUNTIME DESTINATION bin
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
)
install(FILES ${PUBLIC_HEADERS}
  DESTINATION include/portable_concurrency COMPONENT Devel
)
install(FILES ${INSTALL_HEADERS}
  DESTINATION include/portable_concurrency/bits COMPONENT Devel
)
