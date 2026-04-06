#include "bench_utf8_harness.hpp"

extern "C" bool bench_utf8_Slookup4v(const uint8_t *buf, size_t buf_sz);

namespace utf8_bench {
BENCH_UTF8_SUITE(Slookup4v, bench_utf8_Slookup4v);
}

BENCHMARK_MAIN();
