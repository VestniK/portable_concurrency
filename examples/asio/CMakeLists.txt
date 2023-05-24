cmake_minimum_required(VERSION 3.15)

project(asio-pc)

find_package(asio REQUIRED)
find_package(portable_concurrency REQUIRED)

add_executable(asio-pc
  main.cpp

  adapters.h
)
target_link_libraries(asio-pc asio::asio portable_concurrency::portable_concurrency)
target_compile_definitions(asio-pc PRIVATE ASIO_NO_DEPRECATED)
target_compile_features(coroutine PRIVATE
  cxx_std_17
)
