/*
 * Same 64-bit aligned ASCII scan as validate_early_exit.c, but the scalar
 * UTF-8 validator runs only on the suffix starting at the first byte with
 * bit 7 set (shared utf8_slow_scalar_validate).
 *
 * Compare: validate_early_exit.c (slow path rescans from buf[0]).
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "utf8_slow_scalar.h"

/* Smallest i such that buf[i] has high bit set, or buf_sz if all ASCII. */
UTF8_BENCH_INLINE size_t
ascii_prefix_end(const uint8_t* buf, size_t buf_sz)
{
	size_t off = 0;

	/* 1. HEAD: peel until 8-byte aligned */
	uintptr_t addr = (uintptr_t)(buf + off);
	size_t head = (-addr) & 7;

	if (head > buf_sz) {
		head = buf_sz;
	}

	for (size_t i = 0; i < head; i++) {
		if ((buf[off + i] & 0x80) != 0) {
			return off + i;
		}
	}

	off += head;

	/* 2. BULK: aligned 64-bit loop */
	size_t rem = buf_sz - off;
	size_t n64 = rem / 8;
	const uint64_t* p64 = (const uint64_t*)(buf + off);

	for (size_t i = 0; i < n64; i++) {
		const uint64_t w = p64[i];
		const uint64_t t = w & 0x8080808080808080ULL;

		if (t != 0) {
			const size_t base = off + i * 8;
			unsigned j;

#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
			/* BE load: buf[0] is high bits; first 0x80 is the topmost MSB lane. */
			j = (unsigned)__builtin_clzll(t) / 8u;
#else
			/* LE (default if __BYTE_ORDER__ unset): MSBs at 7,15,… */
			j = ((unsigned)__builtin_ctzll(t) - 7u) / 8u;
#endif
			return base + (size_t)j;
		}
	}

	off += n64 * 8;

	/* 3. TAIL: last 0–7 bytes */
	size_t tail = buf_sz - off;

	for (size_t i = 0; i < tail; i++) {
		if ((buf[off + i] & 0x80) != 0) {
			return off + i;
		}
	}

	return buf_sz;
}

bool
as_str_is_valid_utf8(const uint8_t* buf, size_t buf_sz)
{
	if (buf == NULL) {
		return buf_sz == 0;
	}

	const size_t k = ascii_prefix_end(buf, buf_sz);

	if (k == buf_sz) {
		return true;
	}

	return utf8_slow_scalar_validate(buf + k, buf_sz - k);
}
