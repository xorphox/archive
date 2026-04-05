// bench_scalar_EeC_aligned — same as bench_scalar_EeC but uses
// utf8_fast_aligned (noinline+hot) instead of inlined utf8_fast_scalar.

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "utf8_slow_scalar.h"

extern size_t utf8_fast_aligned(const uint8_t *buf, size_t buf_sz);

bool
as_str_is_valid_utf8(const uint8_t *buf, size_t buf_sz)
{
	if (buf == NULL) {
		return buf_sz == 0;
	}

	const size_t k = utf8_fast_aligned(buf, buf_sz);

	if (k == buf_sz) {
		return true;
	}

	return utf8_slow_scalar(buf + k, buf_sz - k);
}
