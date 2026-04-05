/*
 * AVX2 32-byte high-bit scan for leading ASCII, then utf8_slow_scalar on suffix.
 * Build this TU with -march=haswell (or equivalent) so SIMDe/native can use AVX2.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "utf8_bench_inline.h"
#include "utf8_fast_avx2.h"
#include "utf8_slow_scalar.h"

static UTF8_BENCH_NOINLINE bool
utf8_slow_scalar_suffix(const uint8_t *buf, size_t buf_sz)
{
	return utf8_slow_scalar_validate(buf, buf_sz);
}

bool
as_str_is_valid_utf8(const uint8_t *buf, size_t buf_sz)
{
	if (buf == NULL) {
		return buf_sz == 0;
	}

	size_t i = utf8_fast_avx2_ascii_prefix_end(buf, buf_sz);

	if (i == buf_sz) {
		return true;
	}

	return utf8_slow_scalar_suffix(buf + i, buf_sz - i);
}
