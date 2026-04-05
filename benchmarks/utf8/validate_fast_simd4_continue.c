/*
 * Like validate_fast_simd_continue.c, but ASCII chunk test uses SSE4.1
 * simde_mm_test_all_zeros instead of movemask_epi8.
 *
 * Build this TU with at least -msse4.1 (or a Nehalem+ -march) on x86 so the
 * native PTEST path can be used.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <simde/x86/sse4.1.h>

#include "utf8_bench_inline.h"
#include "utf8_slow_scalar.h"

static UTF8_BENCH_NOINLINE bool
utf8_slow_scalar_suffix(const uint8_t *buf, size_t buf_sz)
{
	return utf8_slow_scalar_validate(buf, buf_sz);
}

bool
as_str_is_valid_utf8(const uint8_t* buf, size_t buf_sz)
{
	size_t i = 0;
	simde__m128i highbit = simde_mm_set1_epi8((char)0x80);

	if (buf == NULL) {
		return buf_sz == 0;
	}

	while (i + 16 <= buf_sz) {
		simde__m128i v = simde_mm_loadu_si128(
				(const simde__m128i*)(const void*)(buf + i));
		simde__m128i masked = simde_mm_and_si128(v, highbit);

		/* SSE4.1: true iff (masked & masked) is all zeros — same as movemask==0 */
		if (! simde_mm_test_all_zeros(masked, masked)) {
			break;
		}

		i += 16;
	}

	while (i < buf_sz && buf[i] <= 0x7F) {
		i++;
	}

	if (i == buf_sz) {
		return true;
	}

	return utf8_slow_scalar_suffix(buf + i, buf_sz - i);
}
