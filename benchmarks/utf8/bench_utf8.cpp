// Monolithic bench_utf8: all implementations in one binary (link-order / layout interactions).
// For isolated per-impl binaries see targets bench_utf8_one_* (same BM_* names → merge JSON).

#include "bench_utf8_harness.hpp"

extern "C" {
bool bench_utf8_scalar_EeR(const uint8_t* buf, size_t buf_sz);
bool bench_utf8_scalar_EeC(const uint8_t* buf, size_t buf_sz);
bool bench_utf8_Forig_Sscalar_R(const uint8_t* buf, size_t buf_sz);
bool bench_utf8_Fmemcpy_Sscalar_R(const uint8_t* buf, size_t buf_sz);
bool bench_utf8_Fsingle_Sscalar_R(const uint8_t* buf, size_t buf_sz);
bool bench_utf8_Sscalar(const uint8_t* buf, size_t buf_sz);
bool bench_utf8_Fsse2_Sscalar_EeR(const uint8_t* buf, size_t buf_sz);
bool bench_utf8_Fsse2_Sscalar_EeC(const uint8_t* buf, size_t buf_sz);
#if defined(__x86_64__)
bool bench_utf8_scalar_EeC_haswell(const uint8_t* buf, size_t buf_sz);
bool bench_utf8_Fsse41_Sscalar_EeC(const uint8_t* buf, size_t buf_sz);
bool bench_utf8_Favx2_Sscalar_EeC(const uint8_t* buf, size_t buf_sz);
bool bench_utf8_Favx2_Slookup4_EeC(const uint8_t* buf, size_t buf_sz);
bool bench_utf8_Favx2_Sutf8d_EeC(const uint8_t* buf, size_t buf_sz);
#endif
}

namespace utf8_bench {

// Legacy table column → BM_<impl>_…:
//   early_exit_cont      → scalar_EeC
//   fast_simd_cont       → Fsse2_Sscalar_EeC
//   fast_simd4_cont      → Fsse41_Sscalar_EeC   (x86_64 Haswell)
//   fast_avx2_cont       → Favx2_Sscalar_EeC
//   fast_avx2_lookup4    → Favx2_Slookup4_EeC
//   fast_avx2_utf8d      → Favx2_Sutf8d_EeC
//   scalar_slow          → Sscalar

BENCH_UTF8_SUITE(scalar_EeC, bench_utf8_scalar_EeC);
// BENCH_UTF8_SUITE(scalar_EeR, bench_utf8_scalar_EeR);
BENCH_UTF8_SUITE(Fsse2_Sscalar_EeC, bench_utf8_Fsse2_Sscalar_EeC);
BENCH_UTF8_SUITE(Sscalar, bench_utf8_Sscalar);
#if defined(__x86_64__)
BENCH_UTF8_SUITE(Fsse41_Sscalar_EeC, bench_utf8_Fsse41_Sscalar_EeC);
BENCH_UTF8_SUITE(Favx2_Sscalar_EeC, bench_utf8_Favx2_Sscalar_EeC);
BENCH_UTF8_SUITE(Favx2_Slookup4_EeC, bench_utf8_Favx2_Slookup4_EeC);
BENCH_UTF8_SUITE(Favx2_Sutf8d_EeC, bench_utf8_Favx2_Sutf8d_EeC);
#endif
} // namespace utf8_bench

BENCHMARK_MAIN();
