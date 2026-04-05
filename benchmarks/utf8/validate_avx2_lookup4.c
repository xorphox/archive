/*
 * AVX2 ASCII prefix (utf8_fast_avx2) + Lemire lookup4 scalar suffix (utf8_slow_lookup4).
 * Build this TU with -march=haswell (or equivalent) for the prefix path.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "utf8_fast_avx2.h"
#include "utf8_slow_lookup4.h"

bool
as_str_is_valid_utf8(const uint8_t *buf, size_t buf_sz)
{
	if (buf == NULL) {
		return buf_sz == 0;
	}

	size_t i = utf8_fast_avx2_skip_ascii(buf, buf_sz);

	if (i == buf_sz) {
		return true;
	}

	return utf8_slow_lookup4_validate(buf + i, buf_sz - i);
}
