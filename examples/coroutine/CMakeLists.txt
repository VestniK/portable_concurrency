project(coroutine)
cmake_minimum_required(VERSION 3.8)

set(CMAKE_CXX_STANDARD 17)

find_package(portable_concurrency REQUIRED)
find_package(CoroutinesTS REQUIRED)
add_executable(coroutine
  coroutine.cpp
)
target_link_libraries(coroutine
  portable_concurrency::portable_concurrency
  coroutines::coroutines-ts
)
