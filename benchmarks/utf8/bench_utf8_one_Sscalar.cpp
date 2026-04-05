#include "bench_utf8_harness.hpp"

extern "C" bool bench_utf8_Sscalar(const uint8_t *buf, size_t buf_sz);

namespace utf8_bench {
BENCH_UTF8_SUITE(Sscalar, bench_utf8_Sscalar);
}

BENCHMARK_MAIN();
