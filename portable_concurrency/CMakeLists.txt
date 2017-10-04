find_package(Threads REQUIRED)

set(HEADERS
  functional
  future
  latch

  bits/concurrency_type_traits.h
  bits/future.h
  bits/future_sequence.h
  bits/fwd.h
  bits/invoke.h
  bits/latch.h
  bits/make_future.h
  bits/once_consumable_stack.h
  bits/packaged_task.h
  bits/promise.h
  bits/result_box.h
  bits/shared_future.h
  bits/shared_state.h
  bits/then.h
  bits/type_erasure_owner.h
  bits/unwrappable_state.h
  bits/unique_function.h
  bits/utils.h
  bits/when_any.h
  bits/when_all.h
)

set(SRC
  bits/portable_concurrency.cpp
)

add_library(portable_concurrency ${SRC})
target_sources(portable_concurrency PRIVATE ${HEADERS})
target_link_libraries(portable_concurrency INTERFACE Threads::Threads)
target_include_directories(portable_concurrency INTERFACE
  $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
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
