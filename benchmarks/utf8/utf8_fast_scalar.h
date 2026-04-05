#pragma once

// Scalar prefix scan: end of leading ASCII (aligned 64-bit bulk).

#include <stddef.h>
#include <stdint.h>

#include "utf8_bench_inline.h"

#ifndef UTF8_FAST_SCALAR_INLINE
#define UTF8_FAST_SCALAR_INLINE UTF8_BENCH_HDR_INLINE
#endif

// First high-bit byte index, or buf_sz if all ASCII. buf == NULL → 0.
UTF8_FAST_SCALAR_INLINE size_t
utf8_fast_scalar(const uint8_t *buf, size_t buf_sz)
{
	size_t off = 0;

	if (buf == NULL) {
		return 0;
	}

	// 1. HEAD: peel until 8-byte aligned
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

	// 2. BULK: aligned 64-bit loop
	size_t rem = buf_sz - off;
	size_t n64 = rem / 8;
	const uint64_t *p64 = (const uint64_t *)(buf + off);

#if defined(UTF8_FAST_SCALAR_USE_ALIGNED)
	size_t bulk = utf8_fast_bulk(p64, n64);

	if (bulk < n64 * 8) {
		return off + bulk;
	}
#else
	for (size_t i = 0; i < n64; i++) {
		const uint64_t w = p64[i];
		const uint64_t t = w & 0x8080808080808080ULL;

		if (t != 0) {
			const size_t base = off + i * 8;
			unsigned j;

#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
			j = (unsigned)__builtin_clzll(t) / 8u;
#else
			j = ((unsigned)__builtin_ctzll(t) - 7u) / 8u;
#endif
			return base + (size_t)j;
		}
	}
#endif

	off += n64 * 8;

	// 3. TAIL: last 0–7 bytes
	size_t tail = buf_sz - off;

	for (size_t i = 0; i < tail; i++) {
		if ((buf[off + i] & 0x80) != 0) {
			return off + i;
		}
	}

	return buf_sz;
}
