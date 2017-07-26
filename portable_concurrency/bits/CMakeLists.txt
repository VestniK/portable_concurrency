set(HEADERS
  concurrency_type_traits.h
  continuation.h
  continuation_state.h
  future.h
  future_sequence.h
  fwd.h
  invoke.h
  latch.h
  make_future.h
  once_consumable_stack.h
  packaged_task.h
  promise.h
  result_box.h
  shared_future.h
  shared_state.h
  unwrappable_state.h
  utils.h
  wait_continuation.h
  when_any.h
  when_all.h
)
headeronly_target_headers(portable_concurrency ${HEADERS})

install(FILES ${HEADERS}
  DESTINATION include/concurency/bits COMPONENT Devel
)
