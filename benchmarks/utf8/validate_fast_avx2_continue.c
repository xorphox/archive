/*
 * AVX2 32-byte high-bit scan for leading ASCII, then utf8_scalar_slow on suffix.
 * Build this TU with -march=haswell (or equivalent) so SIMDe/native can use AVX2.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <simde/x86/avx2.h>

#include "utf8_scalar_slow.h"

bool
as_str_is_valid_utf8(const uint8_t* buf, size_t buf_sz)
{
	size_t i = 0;
	simde__m256i highbit = simde_mm256_set1_epi8((char)0x80);

	if (buf == NULL) {
		return buf_sz == 0;
	}

	while (i + 32 <= buf_sz) {
		simde__m256i v = simde_mm256_loadu_si256(
				(const simde__m256i*)(const void*)(buf + i));
		simde__m256i masked = simde_mm256_and_si256(v, highbit);

		if (simde_mm256_movemask_epi8(masked)) {
			break;
		}

		i += 32;
	}

	while (i < buf_sz && buf[i] <= 0x7F) {
		i++;
	}

	if (i == buf_sz) {
		return true;
	}

	return utf8_validate_scalar_slow(buf + i, buf_sz - i);
}
