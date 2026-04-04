/*
 * Like validate_fast_simd_continue: SIMD high-bit scan + scalar byte crawl to skip
 * leading ASCII with no second pass over that prefix (typical client output).
 *
 * From the first non-ASCII byte onward, SSE2 classify + 16-bit structure masks
 * (with continuation carry across chunk boundaries) reject most malformed input
 * quickly; utf8_validate_scalar_slow(buf + suffix, len) then enforces strict
 * UTF-8 (overlongs, range, surrogates, 0xF5–0xF7) only on that suffix — safe
 * against malicious bytes in the non-ASCII region while keeping the hot path cheap.
 */

#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <simde/x86/sse2.h>

#include "utf8_scalar_slow.h"

typedef struct {
	uint16_t cont_mask;
	uint16_t lead2_mask;
	uint16_t lead3_mask;
	uint16_t lead4_mask;
	uint16_t invalid_mask;
} utf8_masks;

static inline utf8_masks
classify_utf8_16(const uint8_t* p)
{
	simde__m128i v = simde_mm_loadu_si128((const simde__m128i*)(const void*)p);

	simde__m128i high4 = simde_mm_and_si128(v, simde_mm_set1_epi8(0xF0));

	simde__m128i cont = simde_mm_cmpeq_epi8(
			simde_mm_and_si128(v, simde_mm_set1_epi8(0xC0)),
			simde_mm_set1_epi8(0x80));

	simde__m128i lead2 = simde_mm_cmpeq_epi8(high4, simde_mm_set1_epi8(0xC0));
	simde__m128i lead3 = simde_mm_cmpeq_epi8(high4, simde_mm_set1_epi8(0xE0));
	simde__m128i lead4 = simde_mm_cmpeq_epi8(high4, simde_mm_set1_epi8(0xF0));

	/* 0xF8–0xFF: (byte & 0xF8) == 0xF8 (unsigned; avoids signed cmpgt quirks) */
	simde__m128i invalid = simde_mm_cmpeq_epi8(
			simde_mm_and_si128(v, simde_mm_set1_epi8(0xF8)),
			simde_mm_set1_epi8((char)0xF8));

	utf8_masks m;

	m.cont_mask = simde_mm_movemask_epi8(cont);
	m.lead2_mask = simde_mm_movemask_epi8(lead2);
	m.lead3_mask = simde_mm_movemask_epi8(lead3);
	m.lead4_mask = simde_mm_movemask_epi8(lead4);
	m.invalid_mask = simde_mm_movemask_epi8(invalid);

	return m;
}

static inline utf8_masks
masks_suffix(utf8_masks m, unsigned skip)
{
	utf8_masks o;

	o.cont_mask = (uint16_t)(m.cont_mask >> skip);
	o.lead2_mask = (uint16_t)(m.lead2_mask >> skip);
	o.lead3_mask = (uint16_t)(m.lead3_mask >> skip);
	o.lead4_mask = (uint16_t)(m.lead4_mask >> skip);
	o.invalid_mask = (uint16_t)(m.invalid_mask >> skip);

	return o;
}

/* Structure for a logical n-byte prefix of a chunk (n <= 16), low n bits only. */
static inline bool
validate_structure_n(utf8_masks m, unsigned n)
{
	uint16_t lim;
	uint16_t need1;
	uint16_t need2;
	uint16_t need3;
	uint16_t s1;
	uint16_t s2;
	uint16_t s3;
	uint16_t all_leads;
	uint16_t valid_cont;

	if (n == 0) {
		return true;
	}

	if (n > 16) {
		return false;
	}

	lim = (uint16_t)((1u << n) - 1u);

	m.cont_mask &= lim;
	m.lead2_mask &= lim;
	m.lead3_mask &= lim;
	m.lead4_mask &= lim;
	m.invalid_mask &= lim;

	if (m.invalid_mask) {
		return false;
	}

	need1 = m.lead2_mask;
	need2 = m.lead3_mask;
	need3 = m.lead4_mask;

	s1 = (uint16_t)((need1 << 1) & lim);

	if (s1 & (uint16_t)~m.cont_mask) {
		return false;
	}

	s2 = (uint16_t)(((need2 << 1) | (need2 << 2)) & lim);

	if (s2 & (uint16_t)~m.cont_mask) {
		return false;
	}

	s3 = (uint16_t)(((need3 << 1) | (need3 << 2) | (need3 << 3)) & lim);

	if (s3 & (uint16_t)~m.cont_mask) {
		return false;
	}

	all_leads = (uint16_t)(m.lead2_mask | m.lead3_mask | m.lead4_mask);
	valid_cont = (uint16_t)(
			((all_leads << 1) |
			(m.lead3_mask << 2) |
			(m.lead4_mask << 2) |
			(m.lead4_mask << 3)) &
			lim);

	if (m.cont_mask & (uint16_t)~valid_cont) {
		return false;
	}

	return true;
}

/*
 * How many bytes after this 16-byte block must be continuation bytes (0–3).
 * Starts decoding at offset `skip` (leading bytes already matched to previous chunk).
 * Returns UINT_MAX on orphan continuation / bad lead at a codepoint boundary.
 */
static unsigned
trailing_cont_needed_from(const uint8_t* c, unsigned skip)
{
	unsigned need = 0;
	size_t j = skip;

	while (j < 16) {
		uint8_t b = c[j];
		int L;

		if (b <= 0x7F) {
			j++;
			continue;
		}

		if ((b & 0xC0) == 0x80) {
			return UINT_MAX;
		}

		if ((b & 0xE0) == 0xC0) {
			L = 2;
		}
		else if ((b & 0xF0) == 0xE0) {
			L = 3;
		}
		else if ((b & 0xF8) == 0xF0) {
			L = 4;
		}
		else {
			return UINT_MAX;
		}

		if (j + (size_t)L > 16) {
			unsigned t = (unsigned)(j + (size_t)L - 16);

			if (t > need) {
				need = t;
			}
		}

		j += (size_t)L;
	}

	return need;
}

bool
as_str_is_valid_utf8(const uint8_t* buf, size_t buf_sz)
{
	unsigned pending = 0;
	size_t i = 0;
	size_t k;
	size_t suffix0;

	if (buf == NULL) {
		return buf_sz == 0;
	}

	/* Same ASCII fast path as validate_fast_simd_continue. */
	while (i + 16 <= buf_sz) {
		simde__m128i v = simde_mm_loadu_si128(
				(const simde__m128i*)(const void*)(buf + i));
		simde__m128i masked = simde_mm_and_si128(
				v, simde_mm_set1_epi8((char)0x80));

		if (simde_mm_movemask_epi8(masked)) {
			break;
		}

		i += 16;
	}

	while (i < buf_sz && buf[i] <= 0x7F) {
		i++;
	}

	if (i == buf_sz) {
		return true;
	}

	suffix0 = i;

	while (i + 16 <= buf_sz) {
		utf8_masks m;
		utf8_masks ms;

		for (k = 0; k < pending; k++) {
			if ((buf[i + k] & 0xC0) != 0x80) {
				return false;
			}
		}

		m = classify_utf8_16(buf + i);
		ms = masks_suffix(m, pending);

		if (! validate_structure_n(ms, 16u - pending)) {
			return false;
		}

		unsigned next = trailing_cont_needed_from(buf + i, pending);

		if (next == UINT_MAX) {
			return false;
		}

		pending = next;
		i += 16;
	}

	if (pending > buf_sz - i) {
		return false;
	}

	for (k = 0; k < pending; k++) {
		if ((buf[i + k] & 0xC0) != 0x80) {
			return false;
		}
	}

	return utf8_validate_scalar_slow(buf + suffix0, buf_sz - suffix0);
}
