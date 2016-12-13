set(HEADERS
  future_error.h
  future.h
  future_status.h
  fwd.h
  make_future.h
  promise.h
  result_box.h
  shared_future.h
  shared_state.h
  utils.h
)

install(FILES ${HEADERS}
  DESTINATION include/concurency/bits COMPONENT Devel
)
