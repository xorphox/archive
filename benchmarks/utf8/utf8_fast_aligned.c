// utf8_fast_aligned — noinline+hot wrapper around utf8_fast_scalar.
//
// Identical algorithm to utf8_fast_scalar.h, but compiled as a standalone
// non-inlined function.  __attribute__((hot)) places it in .text.hot, giving
// it its own section isolated from C++ harness code.  The pragma sets
// -falign-functions=64 so the hot 64-bit ASCII loop lands at a fixed offset
// within a single 32-byte DSB window regardless of what the rest of the
// binary looks like.
//
// Compare bench_utf8_one_scalar_EeCA vs bench_utf8_one_scalar_EeC
// to measure the DSB alignment effect without changing the algorithm.

#include <stddef.h>
#include <stdint.h>

#pragma GCC push_options
#pragma GCC optimize("align-functions=64")

#include "utf8_fast_scalar.h"

__attribute__((noinline, hot)) size_t
utf8_fast_aligned(const uint8_t *buf, size_t buf_sz)
{
	return utf8_fast_scalar(buf, buf_sz);
}

#pragma GCC pop_options
