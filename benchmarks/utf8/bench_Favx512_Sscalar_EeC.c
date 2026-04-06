// AVX-512 64-byte high-bit scan for leading ASCII, then utf8_slow_scalar on suffix.
// Build this TU with -march=skylake-avx512 or -march=znver4.

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define UTF8_BENCH_USE_ALWAYS_INLINE
#include "utf8_fast_avx512.h"
#include "utf8_slow_scalar.h"

bool
as_str_is_valid_utf8(const uint8_t *buf, size_t buf_sz)
{
	if (buf == NULL) {
		return buf_sz == 0;
	}

	size_t i = utf8_fast_avx512(buf, buf_sz);

	if (i == buf_sz) {
		return true;
	}

	return utf8_slow_scalar(buf + i, buf_sz - i);
}
