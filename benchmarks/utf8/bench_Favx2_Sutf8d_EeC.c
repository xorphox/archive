// AVX2 ASCII prefix (utf8_fast_avx2) + Höhrmann utf8d scalar suffix (utf8_slow_utf8d).
// Build this TU with -march=haswell (or equivalent) for the prefix path.

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// UTF8_BENCH_USE_ALWAYS_INLINE: always_inline for utf8_fast_* / utf8_slow_* in this TU.
#define UTF8_BENCH_USE_ALWAYS_INLINE
#include "utf8_fast_avx2.h"
#include "utf8_slow_utf8d.h"

bool
as_str_is_valid_utf8(const uint8_t *buf, size_t buf_sz)
{
	if (buf == NULL) {
		return buf_sz == 0;
	}

	size_t i = utf8_fast_avx2(buf, buf_sz);

	if (i == buf_sz) {
		return true;
	}

	return utf8_slow_utf8d(buf + i, buf_sz - i);
}
