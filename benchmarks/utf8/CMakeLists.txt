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

# SIMDe: #include <simde/x86/sse2.h> (headers live under submodules/simde/simde/)
set(ARCHIVE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../..")
set(SIMDE_INCLUDE_DIR "${ARCHIVE_ROOT}/submodules/simde")

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
  target_include_directories(utf8_lookup4_slow_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${SIMDE_INCLUDE_DIR}"
  )
  target_link_libraries(utf8_lookup4_slow_test PRIVATE GTest::gtest_main)
  target_compile_options(utf8_lookup4_slow_test PRIVATE -Wall)
  if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64)$")
    target_compile_options(utf8_lookup4_slow_test PRIVATE -march=x86-64-v2)
  endif()

  include(GoogleTest)
  # ctest is not a TTY; without this GTest disables ANSI colors (auto mode).
  gtest_discover_tests(utf8_lookup4_slow_test
    PROPERTIES
      ENVIRONMENT "GTEST_COLOR=yes"
  )
endif()

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

# -march=skylake-avx512 for AVX-512 shims.
set(UTF8_BENCH_C_FLAGS_X86_64_AVX512 "")
if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64)$")
  set(UTF8_BENCH_C_FLAGS_X86_64_AVX512
    ${UTF8_BENCH_C_FLAGS_BASE}
    ${UTF8_BENCH_C_FLAGS_GNU_EXTRA}
    -march=skylake-avx512
    -DMARCH_x86_64
  )
endif()

option(UTF8_BENCH_MONOLITH "Build monolithic bench_utf8 (all shims in one binary)" OFF)
option(UTF8_BENCH_AVX512 "Build AVX-512 targets (needs Zen 4+ / Skylake-X+)" OFF)

# Monolithic bench_utf8 (optional): all nocona shims + x86_64 Haswell OBJECT lib.
if(UTF8_BENCH_MONOLITH)
  set(UTF8_BENCH_SHIM_SRCS
    shim/shim_scalar_EeR.c
    shim/shim_scalar_EeC.c
    shim/shim_Forig_Sscalar_R.c
    shim/shim_Fmemcpy_Sscalar_R.c
    shim/shim_Fsingle_Sscalar_R.c
    shim/shim_Sscalar.c
    shim/shim_Fsse2_Sscalar_EeR.c
    shim/shim_Fsse2_Sscalar_EeC.c
  )
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
  target_include_directories(bench_utf8 PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${SIMDE_INCLUDE_DIR}"
  )
  target_link_libraries(bench_utf8 PRIVATE benchmark::benchmark)
  target_compile_options(bench_utf8 PRIVATE
    "$<$<COMPILE_LANGUAGE:C>:${UTF8_BENCH_C_FLAGS}>"
  )
  target_compile_options(bench_utf8 PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-O3>)
endif()

# -----------------------------------------------------------------------------
# Per-implementation binaries (single C shim each): stable layout vs monolithic
# bench_utf8. Same BM_<impl>_… names → JSON from each can be merged for compare:
#   ./bench_utf8_one_scalar_EeC --benchmark_format=json --benchmark_out=a.json
#   jq -s '{benchmarks: [ .[] | .benchmarks[] ]}' a.json b.json > merged.json
# -----------------------------------------------------------------------------

function(utf8_bench_add_one exe cpp shim use_haswell)
  if(use_haswell)
    set(_utf8_one_cflags ${UTF8_BENCH_C_FLAGS_X86_64_HASWELL})
  else()
    set(_utf8_one_cflags ${UTF8_BENCH_C_FLAGS})
  endif()
  add_executable(${exe} ${cpp} ${shim})
  target_include_directories(${exe} PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${SIMDE_INCLUDE_DIR}"
  )
  target_link_libraries(${exe} PRIVATE benchmark::benchmark)
  target_compile_options(${exe} PRIVATE
    "$<$<COMPILE_LANGUAGE:C>:${_utf8_one_cflags}>"
    $<$<COMPILE_LANGUAGE:CXX>:-O3>
  )
endfunction()

#utf8_bench_add_one(bench_utf8_one_scalar_EeC
#  bench_utf8_one_scalar_EeC.cpp shim/shim_scalar_EeC.c OFF)

# "A" (aligned) variants: utf8_fast_scalar compiled as noinline+hot in its own
# .text.hot section with -falign-functions=64 — keeps the tight 64-bit ASCII
# loop in a single 32-byte DSB window regardless of binary layout.
function(utf8_bench_add_one_aligned exe cpp shim)
  add_executable(${exe} ${cpp} ${shim} utf8_fast_aligned.c)
  target_include_directories(${exe} PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${SIMDE_INCLUDE_DIR}"
  )
  target_link_libraries(${exe} PRIVATE benchmark::benchmark)
  target_compile_options(${exe} PRIVATE
    "$<$<COMPILE_LANGUAGE:C>:${UTF8_BENCH_C_FLAGS}>"
    $<$<COMPILE_LANGUAGE:CXX>:-O3>
  )
endfunction()

utf8_bench_add_one_aligned(bench_utf8_one_scalar_EeCA
  bench_utf8_one_scalar_EeCA.cpp shim/shim_scalar_EeCA.c)
utf8_bench_add_one_aligned(bench_utf8_one_Fscalar_Sutf8d_EeCA
  bench_utf8_one_Fscalar_Sutf8d_EeCA.cpp shim/shim_Fscalar_Sutf8d_EeCA.c)
utf8_bench_add_one(bench_utf8_one_Fsse2_Sscalar_EeC
  bench_utf8_one_Fsse2_Sscalar_EeC.cpp shim/shim_Fsse2_Sscalar_EeC.c OFF)
utf8_bench_add_one(bench_utf8_one_Sscalar
  bench_utf8_one_Sscalar.cpp shim/shim_Sscalar.c OFF)

if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64)$")
  #utf8_bench_add_one(bench_utf8_one_Fsse41_Sscalar_EeC
  #  bench_utf8_one_Fsse41_Sscalar_EeC.cpp shim/shim_Fsse41_Sscalar_EeC.c ON)
  utf8_bench_add_one(bench_utf8_one_Favx2_Sscalar_EeC
    bench_utf8_one_Favx2_Sscalar_EeC.cpp shim/shim_Favx2_Sscalar_EeC.c ON)
  utf8_bench_add_one(bench_utf8_one_Favx2_Slookup4_EeC
    bench_utf8_one_Favx2_Slookup4_EeC.cpp shim/shim_Favx2_Slookup4_EeC.c ON)
  utf8_bench_add_one(bench_utf8_one_Favx2_Sutf8d_EeC
    bench_utf8_one_Favx2_Sutf8d_EeC.cpp shim/shim_Favx2_Sutf8d_EeC.c ON)
  utf8_bench_add_one(bench_utf8_one_Slookup4v
    bench_utf8_one_Slookup4v.cpp shim/shim_Slookup4v.c ON)
  utf8_bench_add_one(bench_utf8_one_Favx2_Slookup4v_EeC
    bench_utf8_one_Favx2_Slookup4v_EeC.cpp shim/shim_Favx2_Slookup4v_EeC.c ON)

  add_executable(bench_utf8_one_Fscalar_Slookup4v_EeCA
    bench_utf8_one_Fscalar_Slookup4v_EeCA.cpp
    shim/shim_Fscalar_Slookup4v_EeCA.c
    utf8_fast_aligned.c)
  target_include_directories(bench_utf8_one_Fscalar_Slookup4v_EeCA PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${SIMDE_INCLUDE_DIR}"
  )
  target_link_libraries(bench_utf8_one_Fscalar_Slookup4v_EeCA PRIVATE benchmark::benchmark)
  target_compile_options(bench_utf8_one_Fscalar_Slookup4v_EeCA PRIVATE
    "$<$<COMPILE_LANGUAGE:C>:${UTF8_BENCH_C_FLAGS_X86_64_HASWELL}>"
    $<$<COMPILE_LANGUAGE:CXX>:-O3>
  )

  if(UTF8_BENCH_AVX512)
    add_executable(bench_utf8_one_Favx512_Sscalar_EeC
      bench_utf8_one_Favx512_Sscalar_EeC.cpp shim/shim_Favx512_Sscalar_EeC.c)
    target_include_directories(bench_utf8_one_Favx512_Sscalar_EeC PRIVATE
      "${CMAKE_CURRENT_SOURCE_DIR}"
      "${SIMDE_INCLUDE_DIR}"
    )
    target_link_libraries(bench_utf8_one_Favx512_Sscalar_EeC PRIVATE benchmark::benchmark)
    target_compile_options(bench_utf8_one_Favx512_Sscalar_EeC PRIVATE
      "$<$<COMPILE_LANGUAGE:C>:${UTF8_BENCH_C_FLAGS_X86_64_AVX512}>"
      $<$<COMPILE_LANGUAGE:CXX>:-O3>
    )
  endif()
endif()

set(UTF8_BENCH_ONE_TARGETS
  #bench_utf8_one_scalar_EeC
  bench_utf8_one_scalar_EeCA
  bench_utf8_one_Fsse2_Sscalar_EeC
  bench_utf8_one_Fscalar_Sutf8d_EeCA
  bench_utf8_one_Sscalar
)
if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64)$")
  list(APPEND UTF8_BENCH_ONE_TARGETS
    #bench_utf8_one_Fsse41_Sscalar_EeC
    bench_utf8_one_Favx2_Sscalar_EeC
    bench_utf8_one_Favx2_Slookup4_EeC
    bench_utf8_one_Favx2_Sutf8d_EeC
    bench_utf8_one_Fscalar_Slookup4v_EeCA
    bench_utf8_one_Slookup4v
    bench_utf8_one_Favx2_Slookup4v_EeC
  )
  if(UTF8_BENCH_AVX512)
    list(APPEND UTF8_BENCH_ONE_TARGETS bench_utf8_one_Favx512_Sscalar_EeC)
  endif()
endif()
add_custom_target(bench_utf8_one_all DEPENDS ${UTF8_BENCH_ONE_TARGETS})
