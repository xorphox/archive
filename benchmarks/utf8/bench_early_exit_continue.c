// early_exit_cont: utf8_fast_scalar + utf8_slow_scalar (headers). Define
// UTF8_BENCH_USE_ALWAYS_INLINE below for always_inline on utf8_fast_* / utf8_slow_* in this TU.
// Compare bench_early_exit.c, bench_fast_scalar_utf8d.c.

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define UTF8_BENCH_USE_ALWAYS_INLINE
#include "utf8_fast_scalar.h"
#include "utf8_slow_scalar.h"

bool
as_str_is_valid_utf8(const uint8_t *buf, size_t buf_sz)
{
	if (buf == NULL) {
		return buf_sz == 0;
	}

	const size_t k = utf8_fast_scalar(buf, buf_sz);

	if (k == buf_sz) {
		return true;
	}

	return utf8_slow_scalar(buf + k, buf_sz - k);
}
