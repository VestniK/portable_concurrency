cmake_minimum_required(VERSION 3.15)
project(qt_pc)

set(CMAKE_INCLUDE_CURRENT_DIR On)
set(CMAKE_AUTOMOC On)

find_package(Qt6Core REQUIRED)
find_package(portable_concurrency REQUIRED)

add_executable(qt-pc
  main.cpp
  PCFutureWatcher.cpp
  PCFutureWatcher.h
)
target_link_libraries(qt-pc
  Qt6::Core
  portable_concurrency::portable_concurrency
)
target_compile_features(coroutine PRIVATE
  cxx_std_14
)
