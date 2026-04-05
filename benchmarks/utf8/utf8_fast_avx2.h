#pragma once

/*
 * AVX2 (SIMDe) prefix scan: skip leading bytes that are ASCII (high bit clear).
 * Build TUs that include this with -march=haswell (or MSVC /arch:AVX2) when using
 * native intrinsics; SIMDe lowers for other targets.
 */

#include <stddef.h>
#include <stdint.h>

#include <simde/x86/avx2.h>

#include "utf8_bench_inline.h"

/*
 * Returns the index of the first byte that is not part of a verified ASCII prefix
 * (i.e. first byte with bit 0x80 set), or buf_sz if all bytes are ASCII.
 * buf == NULL is treated like an empty prefix (returns 0).
 */
UTF8_BENCH_INLINE size_t
utf8_fast_avx2_skip_ascii(const uint8_t *buf, size_t buf_sz)
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
