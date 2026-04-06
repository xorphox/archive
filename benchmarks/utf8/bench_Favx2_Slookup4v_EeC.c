// AVX2 ASCII prefix (utf8_fast_avx2) + lookup4 SIMD (SIMDe) validator on suffix.
// Build this TU with -march=haswell (or equivalent).

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define UTF8_BENCH_USE_ALWAYS_INLINE
#include "utf8_fast_avx2.h"
#include "utf8_slow_lookup4v.h"

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

	return utf8_slow_lookup4v(buf + i, buf_sz - i);
}
