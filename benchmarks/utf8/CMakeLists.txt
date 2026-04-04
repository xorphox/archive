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

set(UTF8_SCALAR_SRCS utf8_scalar_slow.c)

set(SHIM_SRCS
  shim_early_exit.c
  shim_early_exit_continue.c
  shim_orig.c
  shim_pre_memcpy.c
  shim_pre_single.c
  shim_scalar_slow.c
  shim_fast_simd_restart.c
  shim_fast_simd_continue.c
  shim_all_simd.c
)

add_executable(bench_utf8 bench_utf8.cpp ${UTF8_SCALAR_SRCS} ${SHIM_SRCS})
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
