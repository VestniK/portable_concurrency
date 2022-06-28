project(qt_pc)
cmake_minimum_required(VERSION 3.12)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_INCLUDE_CURRENT_DIR On)
set(CMAKE_AUTOMOC On)

find_package(Qt5Core REQUIRED)
find_package(portable_concurrency REQUIRED)

add_executable(qt-pc
  main.cpp
  PCFutureWatcher.cpp
  PCFutureWatcher.h
)
target_link_libraries(qt-pc
  Qt5::Core
  portable_concurrency::portable_concurrency
)
