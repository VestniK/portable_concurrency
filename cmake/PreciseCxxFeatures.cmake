# prior cmake 3.8 standard functions to check compilation of snippets
# do not support adjusting C++ standard version. Remove the function
# bellow after requiring cmake >= 3.8
function(check_compiles_cxx14 OUT_VAR SRC)
  if (NOT ${OUT_VAR}_CACHED)
    file(WRITE
      ${CMAKE_CURRENT_BINARY_DIR}/check_${OUT_VAR}/CMakeLists.txt
      "project(check)
      cmake_minimum_required(VERSION 3.3)
      set(CMAKE_CXX_STANDARD 14)
      add_executable(check check.cpp)"
    )
    file(WRITE
      ${CMAKE_CURRENT_BINARY_DIR}/check_${OUT_VAR}/check.cpp
      "${SRC}"
    )
    try_compile(
      _feature_check
      ${CMAKE_CURRENT_BINARY_DIR}/check_${OUT_VAR}/build
      ${CMAKE_CURRENT_BINARY_DIR}/check_${OUT_VAR}
      check
    )
    set(${OUT_VAR}_CACHED ${_feature_check} CACHE BOOL "cached compilation check")
    mark_as_advanced(${OUT_VAR}_CACHED)
  endif()
  set(${OUT_VAR} ${${OUT_VAR}_CACHED} PARENT_SCOPE)
endfunction()

check_compiles_cxx14(
  HAS_STD_ALIGNED_UNION
  "#include <type_traits>
  int main() {return 0;}
  std::aligned_union_t<1, short, double> var;"
)
message(STATUS "Has std::aligned_union_t: ${HAS_STD_ALIGNED_UNION}")

check_compiles_cxx14(
  HAS_STD_ALIGN
  "#include <memory>
  int main() {
    char buf[64];
    std::size_t sz = 64;
    void* buf_ptr = buf;
    void* p = std::align(8, 16, buf_ptr, sz);
    return 0;
  }"
)
message(STATUS "Has std::align: ${HAS_STD_ALIGN}")
