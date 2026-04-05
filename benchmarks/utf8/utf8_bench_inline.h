#ifndef UTF8_BENCH_INLINE_H
#define UTF8_BENCH_INLINE_H

/* static inline helpers; at -O3 the compiler usually inlines into the TU entry point. */
#define UTF8_BENCH_INLINE static inline

/*
 * Outlined cold path: keeps a separate symbol so the ASCII/SIMD prefix loop is not
 * codegen'd in the same huge function as utf8_slow_scalar_validate (fetch / RA pressure).
 */
#if defined(__GNUC__) || defined(__clang__)
#define UTF8_BENCH_NOINLINE __attribute__((noinline))
#else
#define UTF8_BENCH_NOINLINE
#endif

#endif
