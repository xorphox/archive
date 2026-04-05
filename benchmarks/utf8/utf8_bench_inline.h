#pragma once

// static inline helpers; at -O3 the compiler usually inlines into the TU entry point.
#define UTF8_BENCH_INLINE static inline

// Prefer UTF8_BENCH_INLINE for some prefix scans: always_inline can regress ~2× on all-ASCII
// benches. Splitting the hot path across TUs (opaque calls, PLT) can also block folding with
// the caller (~2× class of slowdown vs one visible TU).
#define UTF8_BENCH_ALWAYS_INLINE static inline __attribute__((always_inline))

// Default linkage for utf8_fast_* / utf8_slow_* header functions. Define UTF8_BENCH_USE_ALWAYS_INLINE
// in the .c file before including those headers to switch all of them to always_inline; otherwise
// this is UTF8_BENCH_INLINE. Per-header override: #define UTF8_FAST_SCALAR_INLINE ... before #include.
#ifdef UTF8_BENCH_USE_ALWAYS_INLINE
#define UTF8_BENCH_HDR_INLINE UTF8_BENCH_ALWAYS_INLINE
#else
#define UTF8_BENCH_HDR_INLINE UTF8_BENCH_INLINE
#endif

