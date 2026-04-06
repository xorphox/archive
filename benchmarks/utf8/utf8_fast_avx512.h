#pragma once

// AVX-512 (SIMDe) prefix scan: end of leading ASCII (high bit clear on each byte).
// Build TUs that include this with -march=skylake-avx512 or -march=znver4; SIMDe lowers
// for other targets.

#include <stddef.h>
#include <stdint.h>

#include <simde/x86/avx512/set1.h>
#include <simde/x86/avx512/loadu.h>
#include <simde/x86/avx512/test.h>

#include "utf8_bench_inline.h"

#ifndef UTF8_FAST_AVX512_INLINE
#define UTF8_FAST_AVX512_INLINE UTF8_BENCH_HDR_INLINE
#endif

// First high-bit byte index, or buf_sz if all ASCII. buf == NULL → 0.
UTF8_FAST_AVX512_INLINE size_t
utf8_fast_avx512(const uint8_t *buf, size_t buf_sz)
{
	size_t i = 0;
	simde__m512i highbit = simde_mm512_set1_epi8((char)0x80);

	if (buf == NULL) {
		return 0;
	}

	while (i + 64 <= buf_sz) {
		simde__m512i v = simde_mm512_loadu_si512(
				(const void *)(buf + i));
		simde__mmask64 mask = simde_mm512_test_epi8_mask(v, highbit);

		if (mask) {
			i += (size_t)__builtin_ctzll((uint64_t)mask);
			return i;
		}

		i += 64;
	}

	while (i < buf_sz && buf[i] <= 0x7F) {
		i++;
	}

	return i;
}
