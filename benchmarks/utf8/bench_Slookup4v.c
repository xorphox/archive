// Lookup4 SIMD (SIMDe) validator only — no ASCII prefix scan.

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define UTF8_BENCH_USE_ALWAYS_INLINE
#include "utf8_slow_lookup4v.h"

bool
as_str_is_valid_utf8(const uint8_t *buf, size_t buf_sz)
{
	return utf8_slow_lookup4v(buf, buf_sz);
}
