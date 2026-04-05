/*
 * Fast scalar ASCII prefix (utf8_fast_scalar), then utf8_slow_scalar_validate on suffix.
 *
 * Compare: validate_early_exit.c (slow path rescans from buf[0]).
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "utf8_bench_inline.h"
#include "utf8_fast_scalar.h"
#include "utf8_slow_scalar.h"

/* Suffix only — not inlined into entry so the prefix loop stays a tight function. */
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

	const size_t k = utf8_fast_scalar_ascii_prefix_end(buf, buf_sz);

	if (k == buf_sz) {
		return true;
	}

	return utf8_slow_scalar_suffix(buf + k, buf_sz - k);
}
