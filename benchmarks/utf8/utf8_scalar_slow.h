#ifndef UTF8_SCALAR_SLOW_H
#define UTF8_SCALAR_SLOW_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "utf8_bench_inline.h"

/*
 * Strict UTF-8 scalar validator (multibyte + overlong + surrogates).
 * Header-only static inline so each TU can fold it into callers at -O3 if profitable.
 */
UTF8_BENCH_INLINE bool
utf8_validate_scalar_slow(const uint8_t* buf, size_t buf_sz)
{
	static const uint32_t cp_min[] = { 0, 0x80, 0x800, 0x10000 };

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

#endif
