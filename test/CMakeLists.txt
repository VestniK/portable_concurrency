find_package(GTest REQUIRED)
include_directories(${GTEST_INCLUDE_DIRS})

set(SRC
  future_continuation.cpp
  future.cpp
  once_consumable_queue.cpp
  packaged_task.cpp
  promise.cpp
  shared_future_continuation.cpp
  shared_future.cpp
  test_tools.cpp
  unwrap_constructor.cpp
  when_all_tuple.cpp
  when_any_tuple.cpp
  when_any_vector.cpp
)

add_executable(unit_tests ${SRC})
target_link_libraries(unit_tests concurrency ${GTEST_BOTH_LIBRARIES})
add_test(NAME unit_tests
  COMMAND unit_tests --gtest_output=xml:${CMAKE_BINARY_DIR}/Reports/unit_tests.xml
)

if (ENABLE_UBSAN)
  find_package(UBSan)
  target_link_libraries(unit_tests UBSan)
elseif(ENABLE_ADDRSAN)
  find_package(AddrSan)
  target_link_libraries(unit_tests AddrSan)
elseif(ENABLE_THREADSAN)
  find_package(ThreadSan)
  target_link_libraries(unit_tests ThreadSan)
endif()
