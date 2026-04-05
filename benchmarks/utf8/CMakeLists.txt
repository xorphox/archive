cmake_minimum_required(VERSION 3.18)
project(utf8_bench LANGUAGES C CXX)

# GCC or Clang only (no MSVC / other toolchains).
if(NOT CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  message(FATAL_ERROR "benchmarks/utf8: GCC or Clang required (C compiler is ${CMAKE_C_COMPILER_ID}).")
endif()
if(NOT CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  message(FATAL_ERROR "benchmarks/utf8: GCC or Clang required (C++ compiler is ${CMAKE_CXX_COMPILER_ID}).")
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# C++ driver only; C bench bodies: bench_Ffast_Sslow_Tag.c; Fscalar+Sscalar → bench_scalar_Tag.c.
set(CMAKE_CXX_STANDARD 17)

include(FetchContent)
set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
set(BENCHMARK_ENABLE_GTEST_TESTS OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
  googlebenchmark
  GIT_REPOSITORY https://github.com/google/benchmark.git
  GIT_TAG v1.8.4
)
FetchContent_MakeAvailable(googlebenchmark)

option(UTF8_BUILD_TESTS "Build GTest: utf8_slow_lookup4 vs utf8_slow_scalar" ON)

if(UTF8_BUILD_TESTS)
  set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
  FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.15.2
  )
  FetchContent_MakeAvailable(googletest)
  enable_testing()

  add_executable(utf8_lookup4_slow_test test/utf8_lookup4_slow_test.cc)
  target_include_directories(utf8_lookup4_slow_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
  target_link_libraries(utf8_lookup4_slow_test PRIVATE GTest::gtest_main)
  target_compile_options(utf8_lookup4_slow_test PRIVATE -Wall)

  include(GoogleTest)
  # ctest is not a TTY; without this GTest disables ANSI colors (auto mode).
  gtest_discover_tests(utf8_lookup4_slow_test
    PROPERTIES
      ENVIRONMENT "GTEST_COLOR=yes"
  )
endif()

# SIMDe: #include <simde/x86/sse2.h> (headers live under submodules/simde/simde/)
set(ARCHIVE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../..")
set(SIMDE_INCLUDE_DIR "${ARCHIVE_ROOT}/submodules/simde")

# =============================================================================
# C compiler flags — what goes where
#
#   Target                          | CMake variable / rule
#   ------------------------------- | ------------------------------------------
#   bench_utf8 C shims (default)    | UTF8_BENCH_C_FLAGS — nocona on x86_64
#   bench_utf8 C++ driver           | -O3 only (below)
#   x86_64 Haswell-only shims (5)   | OBJECT lib utf8_bench_x86_haswell — UTF8_BENCH_C_FLAGS_X86_64_HASWELL
#   utf8_lookup4_slow_test          | -Wall (CXX only)
#
# Haswell shims are never mixed with nocona: they live only in utf8_bench_x86_haswell.
# =============================================================================

set(UTF8_BENCH_C_FLAGS_BASE
  -gdwarf-4
  -g3
  # -O3 comes from CMAKE_BUILD_TYPE=Release (matches Aerospike O=3)
  -fno-common
  -fno-strict-aliasing
  -Wall
  -D_FILE_OFFSET_BITS=64
  -std=gnu99
  -D_REENTRANT
  -D_GNU_SOURCE
  -DENHANCED_ALLOC
)

set(UTF8_BENCH_C_FLAGS_GNU_EXTRA "")
if(CMAKE_C_COMPILER_ID STREQUAL "GNU")
  if(CMAKE_C_COMPILER_VERSION VERSION_GREATER_EQUAL 8.0)
    list(APPEND UTF8_BENCH_C_FLAGS_GNU_EXTRA -fno-stack-clash-protection)
  endif()
  if(CMAKE_C_COMPILER_VERSION VERSION_GREATER_EQUAL 9.0)
    list(APPEND UTF8_BENCH_C_FLAGS_GNU_EXTRA -Wno-address-of-packed-member)
  endif()
endif()

# Default C flags for bench_utf8 shim sources (and any other TU on this target).
set(UTF8_BENCH_C_FLAGS ${UTF8_BENCH_C_FLAGS_BASE} ${UTF8_BENCH_C_FLAGS_GNU_EXTRA})
if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64)$")
  list(APPEND UTF8_BENCH_C_FLAGS -march=nocona -DMARCH_x86_64)
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|ARM64)$")
  list(APPEND UTF8_BENCH_C_FLAGS -mcpu=neoverse-n1 -DMARCH_aarch64)
endif()

# Single -march=haswell for all x86_64 shims that need Haswell (SIMDe AVX2, SSE4.1 paths, etc.).
set(UTF8_BENCH_C_FLAGS_X86_64_HASWELL "")
if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64)$")
  set(UTF8_BENCH_C_FLAGS_X86_64_HASWELL
    ${UTF8_BENCH_C_FLAGS_BASE}
    ${UTF8_BENCH_C_FLAGS_GNU_EXTRA}
    -march=haswell
    -DMARCH_x86_64
  )
endif()

option(UTF8_BENCH_FULL_RANGE "Benchmark sizes 8 B … 8 MiB (default: 8 MiB only)" OFF)

# bench_utf8: always these C shims (nocona / neoverse as above).
set(UTF8_BENCH_SHIM_SRCS
  shim/shim_scalar_EeR.c
  shim/shim_scalar_EeC.c
  shim/shim_eec_orig.c
  shim/shim_Forig_Sscalar_R.c
  shim/shim_Fmemcpy_Sscalar_R.c
  shim/shim_Fsingle_Sscalar_R.c
  shim/shim_Sscalar.c
  shim/shim_Fsse2_Sscalar_EeR.c
  shim/shim_Fsse2_Sscalar_EeC.c
  shim/shim_Fscalar_Sutf8d_EeC.c
)

# x86_64 only: linked as OBJECTs — utf8_bench_x86_haswell (Haswell only, no nocona).
set(UTF8_BENCH_X86_HASWELL_SHIMS
  shim/shim_scalar_EeC_haswell.c
  shim/shim_Fsse41_Sscalar_EeC.c
  shim/shim_Favx2_Sscalar_EeC.c
  shim/shim_Favx2_Slookup4_EeC.c
  shim/shim_Favx2_Sutf8d_EeC.c
)

set(UTF8_BENCH_X86_HASWELL_OBJECTS "")
if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64)$")
  add_library(utf8_bench_x86_haswell OBJECT ${UTF8_BENCH_X86_HASWELL_SHIMS})
  target_include_directories(utf8_bench_x86_haswell PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${SIMDE_INCLUDE_DIR}"
  )
  target_compile_options(utf8_bench_x86_haswell PRIVATE
    "$<$<COMPILE_LANGUAGE:C>:${UTF8_BENCH_C_FLAGS_X86_64_HASWELL}>"
  )
  set(UTF8_BENCH_X86_HASWELL_OBJECTS $<TARGET_OBJECTS:utf8_bench_x86_haswell>)
endif()

add_executable(bench_utf8
  bench_utf8.cpp
  ${UTF8_BENCH_SHIM_SRCS}
  ${UTF8_BENCH_X86_HASWELL_OBJECTS}
)
if(UTF8_BENCH_FULL_RANGE)
  target_compile_definitions(bench_utf8 PRIVATE BENCH_UTF8_FULL_RANGE=1)
endif()
target_include_directories(bench_utf8 PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}"
  "${SIMDE_INCLUDE_DIR}"
)
target_link_libraries(bench_utf8 PRIVATE benchmark::benchmark)

target_compile_options(bench_utf8 PRIVATE
  "$<$<COMPILE_LANGUAGE:C>:${UTF8_BENCH_C_FLAGS}>"
)

target_compile_options(bench_utf8 PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-O3>)
