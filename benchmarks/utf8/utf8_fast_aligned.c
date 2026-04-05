// utf8_fast_aligned — DSB-stable bulk ASCII scan.
//
// Splits the hot 64-bit loop out of utf8_fast_scalar into its own noinline+hot
// function with explicit .p2align 5, so the loop always lands in a single
// 32-byte DSB window.  The head-peel and tail remain inlined in the caller
// (utf8_fast_scalar_aligned) — changes to them can't shift the hot loop.
//
// Compare bench_utf8_one_scalar_EeCA vs bench_utf8_one_scalar_EeC
// to measure the DSB alignment effect without changing the algorithm.

#include <stddef.h>
#include <stdint.h>

#pragma GCC push_options
#pragma GCC optimize("align-functions=64")

// Scan aligned 64-bit words for any high bit.  Returns the byte offset of the
// first non-ASCII byte relative to p64, or n64*8 if all words are ASCII.
__attribute__((noinline, hot)) static size_t
utf8_fast_bulk(const uint64_t *p64, size_t n64)
{
	if (n64 == 0) {
		return 0;
	}

	size_t total = n64 * 8;
	size_t off = 0;
	const uint64_t mask = 0x8080808080808080ULL;

	__asm__ volatile (".p2align 5" ::: "memory");
	do {
		const uint64_t t = p64[off / 8] & mask;

		if (t != 0) {
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
			unsigned j = (unsigned)__builtin_clzll(t) / 8u;
#else
			unsigned j = ((unsigned)__builtin_ctzll(t) - 7u) / 8u;
#endif
			return off + (size_t)j;
		}

		off += 8;
	} while (off < total);

	return total;
}

#pragma GCC pop_options

#include "utf8_bench_inline.h"

// Full fast-path: head peel + aligned bulk + tail.  Inlined into the caller.
static inline size_t
utf8_fast_scalar_aligned(const uint8_t *buf, size_t buf_sz)
{
	size_t off = 0;

	if (buf == NULL) {
		return 0;
	}

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

	size_t rem = buf_sz - off;
	size_t n64 = rem / 8;
	size_t bulk = utf8_fast_bulk((const uint64_t *)(buf + off), n64);

	if (bulk < n64 * 8) {
		return off + bulk;
	}

	off += n64 * 8;

	size_t tail = buf_sz - off;

	for (size_t i = 0; i < tail; i++) {
		if ((buf[off + i] & 0x80) != 0) {
			return off + i;
		}
	}

	return buf_sz;
}

size_t
utf8_fast_aligned(const uint8_t *buf, size_t buf_sz)
{
	return utf8_fast_scalar_aligned(buf, buf_sz);
}
