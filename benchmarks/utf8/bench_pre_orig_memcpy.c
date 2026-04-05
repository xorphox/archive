#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stddef.h>
#include <stddef.h>
#include <string.h>

#include "utf8_slow_scalar.h"

UTF8_BENCH_INLINE uint64_t
accumulate64(const uint8_t* buf, size_t buf_sz)
{
	uint64_t acc = 0;
	size_t i = 0;

	for (; i + 8 <= buf_sz; i += 8) {
		uint64_t chunk;
		memcpy(&chunk, buf + i, 8);
		acc |= chunk;
	}

	for (; i < buf_sz; i++) {
		acc |= buf[i];
	}

	return acc;
}

bool
as_str_is_valid_utf8(const uint8_t* buf, size_t buf_sz)
{
	if (buf == NULL) {
		return buf_sz == 0;
	}

	if ((accumulate64(buf, buf_sz) & 0x8080808080808080ULL) == 0) {
		return true;
	}

	return utf8_slow_scalar(buf, buf_sz);
}
