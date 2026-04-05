// SIMD “all ASCII?” fast path; if it fails, scalar UTF-8 validation restarts from buf[0] over
// the *entire* buffer (ASCII prefix is visited again).
// Compare: bench_Fsse2_Sscalar_EeC.c (C: slow path only on suffix; this file is R: full-buffer slow).

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <simde/x86/sse2.h>

#include "utf8_slow_scalar.h"

bool
as_str_is_valid_utf8(const uint8_t* buf, size_t buf_sz)
{
	size_t i = 0;

	if (buf == NULL) {
		return buf_sz == 0;
	}

	for (; i + 16 <= buf_sz; i += 16) {
		simde__m128i v = simde_mm_loadu_si128(
				(const simde__m128i*)(const void*)(buf + i));
		if (simde_mm_movemask_epi8(v) != 0) {
			return utf8_slow_scalar(buf, buf_sz);
		}
	}

	for (; i < buf_sz; i++) {
		if ((buf[i] & 0x80) != 0) {
			return utf8_slow_scalar(buf, buf_sz);
		}
	}

	return true;
}
