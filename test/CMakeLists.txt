find_package(GTest REQUIRED)
include_directories(${GTEST_INCLUDE_DIRS})

set(SRC
  tests.cpp
)

add_executable(unit_tests ${SRC})
target_link_libraries(unit_tests concurrency ${GTEST_BOTH_LIBRARIES})
add_test(NAME unit_tests
  COMMAND unit_tests --gtest_output=xml:${CMAKE_BINARY_DIR}/Reports/unit_tests.xml
)
