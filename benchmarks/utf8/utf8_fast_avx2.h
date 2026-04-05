#pragma once

// AVX2 (SIMDe) prefix scan: end of leading ASCII (high bit clear on each byte).
// Build TUs that include this with -march=haswell when using native intrinsics; SIMDe lowers
// for other targets.

#include <stddef.h>
#include <stdint.h>

#include <simde/x86/avx2.h>

#include "utf8_bench_inline.h"

#ifndef UTF8_FAST_AVX2_INLINE
#define UTF8_FAST_AVX2_INLINE UTF8_BENCH_HDR_INLINE
#endif

// First high-bit byte index, or buf_sz if all ASCII. buf == NULL → 0.
UTF8_FAST_AVX2_INLINE size_t
utf8_fast_avx2(const uint8_t *buf, size_t buf_sz)
{
	size_t i = 0;
	simde__m256i highbit = simde_mm256_set1_epi8((char)0x80);

	if (buf == NULL) {
		return 0;
	}

	while (i + 32 <= buf_sz) {
		simde__m256i v = simde_mm256_loadu_si256(
				(const simde__m256i *)(const void *)(buf + i));
		simde__m256i masked = simde_mm256_and_si256(v, highbit);

		if (simde_mm256_movemask_epi8(masked)) {
			break;
		}

		i += 32;
	}

	while (i < buf_sz && buf[i] <= 0x7F) {
		i++;
	}

	return i;
}
