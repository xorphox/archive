#include "bench_utf8_harness.hpp"

extern "C" bool bench_utf8_Fscalar_Sutf8d_EeCA(const uint8_t *buf, size_t buf_sz);

namespace utf8_bench {
BENCH_UTF8_SUITE(Fscalar_Sutf8d_EeCA, bench_utf8_Fscalar_Sutf8d_EeCA);
}

BENCHMARK_MAIN();
