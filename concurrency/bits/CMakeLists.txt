set(HEADERS
  async.h
  continuation.h
  future.h
  fwd.h
  invoke.h
  make_future.h
  packaged_task.h
  promise.h
  result_box.h
  shared_future.h
  shared_state.h
  utils.h
)

install(FILES ${HEADERS}
  DESTINATION include/concurency/bits COMPONENT Devel
)
