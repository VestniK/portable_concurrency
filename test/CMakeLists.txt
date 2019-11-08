find_package(GTest REQUIRED)
include_directories(${GTEST_INCLUDE_DIRS})

set(API_TESTS
  abandon.cpp
  algo_adapters.cpp
  async.cpp
  cancelation.cpp
  future.cpp
  future_next.cpp
  future_then.cpp
  future_then_unwrap.cpp
  notify.cpp
  packaged_task.cpp
  packaged_task_unwrap.cpp
  promise.cpp
  small_unique_function.cpp
  shared_future.cpp
  shared_future_next.cpp
  shared_future_then.cpp
  shared_future_then_unwrap.cpp
  thread_pool.cpp
  timed_waiter.cpp
  unique_function.cpp
  when_all_tuple.cpp
  when_all_vector.cpp
  when_any_tuple.cpp
  when_any_vector.cpp
)

set(DETAILS_TESTS
  either.cpp
  once_consumable_stack.cpp
)

set(TEST_TOOLS
  test_helpers.h
  test_tools.h
  test_tools.cpp
  simple_arena_allocator.h
)

add_executable(pc_unit_tests ${API_TESTS} ${DETAILS_TESTS} ${TEST_TOOLS})
target_link_libraries(pc_unit_tests portable_concurrency GTest::GTest GTest::Main)
if (CMAKE_CXX_COMPILER_ID MATCHES "GNU")
  if(CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 7)
    # Useless warning on correct code which marks the problem which can only happen
    # when linking code compiled with -std=c++14 and -std=c++17 flags together.
    target_compile_options(pc_unit_tests PRIVATE -Wno-noexcept-type)
  endif()
  if (NOT PC_DEV_BUILD)
    target_compile_options(pc_unit_tests PRIVATE -Wno-unused-result)
  endif()
endif()
if (MSVC)
  target_compile_definitions(pc_unit_tests PRIVATE _CRT_SECURE_NO_WARNINGS)
else()
  if (PC_NO_DEPRECATED)
    target_compile_options(pc_unit_tests PRIVATE -Werror=deprecated-declarations)
  else()
    target_compile_options(pc_unit_tests PRIVATE -Wno-deprecated-declarations)
  endif()
endif()
add_test(NAME pc_unit_tests
  COMMAND pc_unit_tests --gtest_output=xml:${CMAKE_BINARY_DIR}/Reports/pc_unit_tests.xml
)
