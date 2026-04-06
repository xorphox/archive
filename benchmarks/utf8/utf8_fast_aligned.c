// utf8_fast_aligned — DSB-stable bulk ASCII scan (x86_64 only).
//
// Defines utf8_fast_bulk (noinline+hot, .p2align 5) then includes
// utf8_fast_scalar.h with UTF8_FAST_SCALAR_USE_ALIGNED so the header
// calls utf8_fast_bulk instead of its inline loop.  Head-peel and tail
// come from the header — single source of truth, no duplication.
//
// On ARM/aarch64, DSB alignment is not an issue — don't define
// UTF8_FAST_SCALAR_USE_ALIGNED and this file compiles to nothing.
//
// Compare bench_utf8_one_scalar_EeCA vs bench_utf8_one_scalar_EeC
// to measure the DSB alignment effect without changing the algorithm.

#if defined(__x86_64__) || defined(__i386__)

#include <stddef.h>
#include <stdint.h>

#pragma GCC push_options
#pragma GCC optimize("align-functions=64")

// Scan aligned 64-bit words for any high bit.  Returns the byte offset of the
// first non-ASCII byte relative to p64, or n64*8 if all words are ASCII.
__attribute__((noinline, hot)) size_t
utf8_fast_bulk(const uint64_t *p64, size_t n64)
{
	if (n64 == 0) {
		return 0;
	}

	const uint64_t mask = 0x8080808080808080ULL;

	__asm__ volatile (".p2align 5" ::: "memory");

	for (size_t i = 0; i < n64; i++) {
		const uint64_t t = p64[i] & mask;

		if (t != 0) {
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
			unsigned j = (unsigned)__builtin_clzll(t) / 8u;
#else
			unsigned j = ((unsigned)__builtin_ctzll(t) - 7u) / 8u;
#endif
			return i * 8 + (size_t)j;
		}
	}

	return n64 * 8;
}

#pragma GCC pop_options

#endif // x86_64 || i386
