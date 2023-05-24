project(coroutine)
cmake_minimum_required(VERSION 3.15)

find_package(portable_concurrency REQUIRED)
add_executable(coroutine
  coro_timer.cpp
  coro_timer.h
  main.cpp
)
target_link_libraries(coroutine PRIVATE
  portable_concurrency::portable_concurrency
)
target_compile_features(coroutine PRIVATE
  cxx_std_20
)
