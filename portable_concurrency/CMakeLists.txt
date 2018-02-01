find_package(Threads REQUIRED)

set(PUBLIC_HEADERS
  execution
  functional
  functional_fwd
  future
  latch
)
set(INSTALL_HEADERS
  bits/alias_namespace.h
  bits/align.h
  bits/allocate_unique.h
  bits/async.h
  bits/concurrency_type_traits.h
  bits/either.h
  bits/execution.h
  bits/future.h
  bits/future_sequence.h
  bits/future_state.h
  bits/fwd.h
  bits/invoke.h
  bits/latch.h
  bits/make_future.h
  bits/once_consumable_stack_fwd.h
  bits/once_consumable_stack.h
  bits/packaged_task.h
  bits/promise.h
  bits/shared_future.h
  bits/shared_state.h
  bits/then.h
  bits/unique_function.h
  bits/unique_function.hpp
  bits/utils.h
  bits/when_any.h
  bits/when_all.h
)

set(HEADERS
  ${PUBLIC_HEADERS}
  ${INSTALL_HEADERS}
)

set(SRC
  bits/portable_concurrency.cpp
)

add_library(portable_concurrency ${SRC})
target_sources(portable_concurrency PRIVATE ${HEADERS})
target_link_libraries(portable_concurrency Threads::Threads)
target_include_directories(portable_concurrency INTERFACE
  $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
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
