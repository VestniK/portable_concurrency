set(HEADERS
  future_error.h
  future.h
  future_status.h
  promise.h
  result_box.h
  shared_state.h
)

install(FILES ${HEADERS}
  DESTINATION include/concurency/bits COMPONENT Devel
)
