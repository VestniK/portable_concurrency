if (${CMAKE_CXX_COMPILER_ID} STREQUAL "MSVC")
  message(FATAL_ERROR "Address sanitizer is not supported for VisualStudio C++ compiler")
endif()

add_library(AddrSan INTERFACE)
target_compile_options(AddrSan INTERFACE -fsanitize=address -fno-sanitize-recover=all)
target_link_libraries(AddrSan INTERFACE -fsanitize=address -fno-sanitize-recover=all)
