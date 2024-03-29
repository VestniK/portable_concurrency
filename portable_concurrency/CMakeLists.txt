find_package(Threads REQUIRED)

set(PUBLIC_HEADERS
  execution
  functional
  functional_fwd
  future
  future_fwd
  latch
  timed_waiter
  thread_pool
)
set(INSTALL_HEADERS
  bits/algo_adapters.h
  bits/alias_namespace.h
  bits/async.h
  bits/closable_queue.h
  bits/concurrency_type_traits.h
  bits/config.h
  bits/continuations_stack.h
  bits/coro.h
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
  bits/timed_waiter.h
  bits/then.hpp
  bits/thread_pool.h
  bits/unique_function.h
  bits/unique_function.hpp
  bits/utils.h
  bits/voidify.h
  bits/when_any.h
  bits/when_all.h
)

set(HEADERS
  ${PUBLIC_HEADERS}
  ${INSTALL_HEADERS}

  bits/closable_queue.hpp
  bits/once_consumable_stack.hpp
)

set(SRC
  bits/portable_concurrency.cpp
)

add_library(portable_concurrency ${SRC})
target_sources(portable_concurrency PRIVATE ${HEADERS})
target_link_libraries(portable_concurrency Threads::Threads)
target_include_directories(portable_concurrency PUBLIC
  $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
target_compile_features(portable_concurrency PUBLIC cxx_std_14)
if (PC_NO_DEPRECATED)
  target_compile_definitions(portable_concurrency PUBLIC PC_NO_DEPRECATED)
endif()

install(TARGETS portable_concurrency EXPORT portable_concurrency
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
install(FILES ${PUBLIC_HEADERS} DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/${PROJECT_NAME})
install(FILES ${INSTALL_HEADERS}  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/${PROJECT_NAME}/bits)
