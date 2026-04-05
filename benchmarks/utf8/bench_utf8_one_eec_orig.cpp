#include "bench_utf8_harness.hpp"

extern "C" bool bench_utf8_eec_orig(const uint8_t *buf, size_t buf_sz);

namespace utf8_bench {
BENCH_UTF8_SUITE(eec_orig, bench_utf8_eec_orig);
}

BENCHMARK_MAIN();
