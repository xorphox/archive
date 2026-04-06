#include "bench_utf8_harness.hpp"

extern "C" bool bench_utf8_Favx512_Sscalar_EeC(const uint8_t *buf, size_t buf_sz);

namespace utf8_bench {
BENCH_UTF8_SUITE(Favx512_Sscalar_EeC, bench_utf8_Favx512_Sscalar_EeC);
}

BENCHMARK_MAIN();
