#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stddef.h>
#include <stddef.h>
#include <string.h>

static inline uint64_t
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

	static const uint32_t cp_min[] = { 0, 0x80, 0x800, 0x10000 };

	// Slow path: validate multi-byte sequences.
	for (size_t i = 0; i < buf_sz; i++) {
		uint8_t b = buf[i];

		if (b <= 0x7F) {
			continue;
		}

		uint32_t cp;
		uint32_t n;

		if ((b & 0xE0) == 0xC0) {
			cp = b & 0x1F;
			n = 1;
		}
		else if ((b & 0xF0) == 0xE0) {
			cp = b & 0x0F;
			n = 2;
		}
		else if ((b & 0xF8) == 0xF0) {
			cp = b & 0x07;
			n = 3;
		}
		else {
			return false;
		}

		if (i + n >= buf_sz) {
			return false;
		}

		for (uint32_t j = 0; j < n; j++) {
			uint8_t c = buf[++i];

			if ((c & 0xC0) != 0x80) {
				return false;
			}

			cp = (cp << 6) | (c & 0x3F);
		}

		if (cp < cp_min[n] || cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF)) {
			return false;
		}
	}

	return true;
}
