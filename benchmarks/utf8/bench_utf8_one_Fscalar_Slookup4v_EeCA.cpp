#include "bench_utf8_harness.hpp"

extern "C" bool bench_utf8_Fscalar_Slookup4v_EeCA(const uint8_t *buf, size_t buf_sz);

namespace utf8_bench {
BENCH_UTF8_SUITE(Fscalar_Slookup4v_EeCA, bench_utf8_Fscalar_Slookup4v_EeCA);
}

BENCHMARK_MAIN();
