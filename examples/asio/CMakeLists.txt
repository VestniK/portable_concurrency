cmake_minimum_required(VERSION 3.8)

project(asio-pc)

set(CMAKE_CXX_STANDARD 17)

# Assumes standalone asio with pull request applied:
# https://github.com/chriskohlhoff/asio/pull/128
find_package(asio REQUIRED)
find_package(portable_concurrency REQUIRED)

add_executable(asio-pc
  main.cpp

  adapters.h
)
target_link_libraries(asio-pc asio::standalone portable_concurrency::portable_concurrency)
target_compile_definitions(asio-pc PRIVATE ASIO_NO_DEPRECATED)
