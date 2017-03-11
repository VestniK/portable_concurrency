if (${CMAKE_CXX_COMPILER_ID} STREQUAL "MSVC")
  message(FATAL_ERROR "Thread sanitizer is not supported for VisualStudio C++ compiler")
endif()

add_library(ThreadSan INTERFACE)
target_compile_options(ThreadSan INTERFACE -fsanitize=thread -fno-sanitize-recover=all)
target_link_libraries(ThreadSan INTERFACE -fsanitize=thread -fno-sanitize-recover=all)
