// Fast scalar ASCII prefix (utf8_fast_aligned, noinline+hot) + Höhrmann utf8d on suffix.

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "utf8_slow_utf8d.h"

extern size_t utf8_fast_aligned(const uint8_t *buf, size_t buf_sz);

bool
as_str_is_valid_utf8(const uint8_t *buf, size_t buf_sz)
{
	if (buf == NULL) {
		return buf_sz == 0;
	}

	size_t i = utf8_fast_aligned(buf, buf_sz);

	if (i == buf_sz) {
		return true;
	}

	return utf8_slow_utf8d(buf + i, buf_sz - i);
}
