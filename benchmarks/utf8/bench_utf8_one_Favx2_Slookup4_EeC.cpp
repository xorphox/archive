#include "bench_utf8_harness.hpp"

extern "C" bool bench_utf8_Favx2_Slookup4_EeC(const uint8_t *buf, size_t buf_sz);

namespace utf8_bench {
BENCH_UTF8_SUITE(Favx2_Slookup4_EeC, bench_utf8_Favx2_Slookup4_EeC);
}

BENCHMARK_MAIN();
