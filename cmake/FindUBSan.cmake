if (${CMAKE_CXX_COMPILER_ID} STREQUAL "MSVC")
  message(FATAL_ERROR "Undefined Behavior sanitizer is not supported for VisualStudio C++ compiler")
endif()

add_library(UBSan INTERFACE)
target_compile_options(UBSan INTERFACE -fsanitize=undefined -fno-sanitize-recover=all)
target_link_libraries(UBSan INTERFACE -fsanitize=undefined -fno-sanitize-recover=all)
