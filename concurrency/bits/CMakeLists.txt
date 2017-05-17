set(HEADERS
  concurrency_type_traits.h
  continuation.h
  future.h
  fwd.h
  invoke.h
  latch.h
  make_future.h
  once_consumable_queue.h
  packaged_task.h
  promise.h
  result_box.h
  shared_future.h
  shared_state.h
  utils.h
  wait_continuation.h
  when_any.h
)

install(FILES ${HEADERS}
  DESTINATION include/concurency/bits COMPONENT Devel
)
