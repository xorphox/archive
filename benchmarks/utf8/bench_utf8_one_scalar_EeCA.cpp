#include "bench_utf8_harness.hpp"

extern "C" bool bench_utf8_scalar_EeCA(const uint8_t *buf, size_t buf_sz);

namespace utf8_bench {
BENCH_UTF8_SUITE(scalar_EeCA, bench_utf8_scalar_EeCA);
}

BENCHMARK_MAIN();
