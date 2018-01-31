find_package(GTest REQUIRED)
include_directories(${GTEST_INCLUDE_DIRS})

set(SRC
  async.cpp
  cancelation.cpp
  either.cpp
  future.cpp
  future_next.cpp
  future_then.cpp
  once_consumable_stack.cpp
  packaged_task.cpp
  promise.cpp
  shared_future.cpp
  shared_future_next.cpp
  shared_future_then.cpp
  shared_future_then_unwrap.cpp
  test_tools.cpp
  unique_function.cpp
  future_then_unwrap.cpp
  when_all_tuple.cpp
  when_all_vector.cpp
  when_any_tuple.cpp
  when_any_vector.cpp
)

set(HEADERS
  closable_queue.h
  task.h
  test_helpers.h
  test_tools.h
  simple_arena_allocator.h
)

add_executable(unit_tests ${SRC} ${HEADERS})
target_link_libraries(unit_tests portable_concurrency GTest::GTest GTest::Main)
if (CMAKE_CXX_COMPILER_ID MATCHES "GNU")
  # Useless warning on correct code which marks the problem which can only happen
  # when linking code compiled with -std=c++14 and -std=c++17 flags together.
  target_compile_options(unit_tests PRIVATE -Wno-noexcept-type)
endif()
if (MSVC)
  target_compile_definitions(unit_tests PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()
add_test(NAME unit_tests
  COMMAND unit_tests --gtest_output=xml:${CMAKE_BINARY_DIR}/Reports/unit_tests.xml
)
