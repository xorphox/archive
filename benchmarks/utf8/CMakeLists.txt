cmake_minimum_required(VERSION 3.18)
project(utf8_validate_bench LANGUAGES C CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# C++ driver only; C validators follow Aerospike server (make_in/Makefile.in) style.
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

option(UTF8_BUILD_TESTS "Build GTest: utf8_validate_lookup4_slow vs utf8_validate_scalar_slow" ON)

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
  if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(utf8_lookup4_slow_test PRIVATE -Wall)
  endif()

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

# Match aerospike-server make_in/Makefile.in: COMMON_CFLAGS + AS_CFLAGS for C (no -Werror / -MMD).
set(AS_C_FLAGS
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
if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64)$")
  list(APPEND AS_C_FLAGS -march=nocona -DMARCH_x86_64)
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|ARM64)$")
  # Makefile.in aarch64 branch (TSO plugin omitted here).
  list(APPEND AS_C_FLAGS -mcpu=neoverse-n1 -DMARCH_aarch64)
endif()
if(CMAKE_C_COMPILER_ID STREQUAL "GNU")
  if(CMAKE_C_COMPILER_VERSION VERSION_GREATER_EQUAL 8.0)
    list(APPEND AS_C_FLAGS -fno-stack-clash-protection)
  endif()
  if(CMAKE_C_COMPILER_VERSION VERSION_GREATER_EQUAL 9.0)
    list(APPEND AS_C_FLAGS -Wno-address-of-packed-member)
  endif()
endif()

option(UTF8_BENCH_FULL_RANGE "Benchmark sizes 8 B … 8 MiB (default: 8 MiB only)" OFF)

set(SHIM_SRCS
  shim_early_exit.c
  shim_early_exit_continue.c
  shim_orig.c
  shim_pre_memcpy.c
  shim_pre_single.c
  shim_scalar_slow.c
  shim_fast_simd_restart.c
  shim_fast_simd_continue.c
)

# SIMD4 / AVX2 shims: x86_64 only, both TUs compiled with Haswell-class ISA.
if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64)$")
  list(APPEND SHIM_SRCS shim_early_exit_continue_haswell.c)
  list(APPEND SHIM_SRCS shim_fast_simd4_continue.c)
  list(APPEND SHIM_SRCS shim_fast_avx2_continue.c)
endif()

add_executable(bench_utf8 bench_utf8.cpp ${SHIM_SRCS})
if(UTF8_BENCH_FULL_RANGE)
  target_compile_definitions(bench_utf8 PRIVATE BENCH_UTF8_FULL_RANGE=1)
endif()
target_include_directories(bench_utf8 PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}"
  "${SIMDE_INCLUDE_DIR}"
)
target_link_libraries(bench_utf8 PRIVATE benchmark::benchmark)

target_compile_options(bench_utf8 PRIVATE
  "$<$<COMPILE_LANGUAGE:C>:${AS_C_FLAGS}>"
)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(bench_utf8 PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-O3>)
endif()

if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64)$")
  if(MSVC)
    # Haswell-class ISA for both TUs (SSE4.1-only code still fine under AVX2 arch on MSVC).
    set_source_files_properties(shim_early_exit_continue_haswell.c PROPERTIES COMPILE_OPTIONS "/arch:AVX2")
    set_source_files_properties(shim_fast_simd4_continue.c PROPERTIES COMPILE_OPTIONS "/arch:AVX2")
    set_source_files_properties(shim_fast_avx2_continue.c PROPERTIES COMPILE_OPTIONS "/arch:AVX2")
  else()
    set_source_files_properties(shim_early_exit_continue_haswell.c PROPERTIES COMPILE_OPTIONS "-march=haswell")
    set_source_files_properties(shim_fast_simd4_continue.c PROPERTIES COMPILE_OPTIONS "-march=haswell")
    set_source_files_properties(shim_fast_avx2_continue.c PROPERTIES COMPILE_OPTIONS "-march=haswell")
  endif()
endif()
