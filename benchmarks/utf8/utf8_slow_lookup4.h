#pragma once

/*
 * Scalar transcription of Daniel Lemire’s lookup4 UTF-8 validator from
 * validateutf8-experiments (not simdjson’s tree — same math as upstream):
 *   https://github.com/lemire/validateutf8-experiments/blob/master/src/generic/utf8_lookup4_algorithm.h
 *   https://github.com/lemire/validateutf8-experiments/blob/master/src/generic/validator.h
 *
 * Three 16-byte lookup tables (high nibble of prev byte, low nibble of prev,
 * high nibble of current) + must_be_2_3_continuation + 64-byte block + EOF
 * incomplete tail, matching the reference checker.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "utf8_bench_inline.h"

/* Bit masks — names/comments from Lemire’s header */
#define L4_TOO_SHORT     (1u << 0)
#define L4_TOO_LONG      (1u << 1)
#define L4_OVERLONG_3    (1u << 2)
#define L4_TOO_LARGE     (1u << 3)
#define L4_SURROGATE     (1u << 4)
#define L4_OVERLONG_2    (1u << 5)
#define L4_TOO_LARGE_1000 (1u << 6)
#define L4_TWO_CONTS     (1u << 7)
#define L4_OVERLONG_4    (1u << 6)

#define L4_CARRY (L4_TOO_SHORT | L4_TOO_LONG | L4_TWO_CONTS)

static const uint8_t lookup4_byte_1_high[16] = {
	L4_TOO_LONG, L4_TOO_LONG, L4_TOO_LONG, L4_TOO_LONG,
	L4_TOO_LONG, L4_TOO_LONG, L4_TOO_LONG, L4_TOO_LONG,
	L4_TWO_CONTS, L4_TWO_CONTS, L4_TWO_CONTS, L4_TWO_CONTS,
	(uint8_t)(L4_TOO_SHORT | L4_OVERLONG_2),
	L4_TOO_SHORT,
	(uint8_t)(L4_TOO_SHORT | L4_OVERLONG_3 | L4_SURROGATE),
	(uint8_t)(L4_TOO_SHORT | L4_TOO_LARGE | L4_TOO_LARGE_1000 | L4_OVERLONG_4),
};

static const uint8_t lookup4_byte_1_low[16] = {
	(uint8_t)(L4_CARRY | L4_OVERLONG_3 | L4_OVERLONG_2 | L4_OVERLONG_4),
	(uint8_t)(L4_CARRY | L4_OVERLONG_2),
	L4_CARRY,
	L4_CARRY,
	(uint8_t)(L4_CARRY | L4_TOO_LARGE),
	(uint8_t)(L4_CARRY | L4_TOO_LARGE | L4_TOO_LARGE_1000),
	(uint8_t)(L4_CARRY | L4_TOO_LARGE | L4_TOO_LARGE_1000),
	(uint8_t)(L4_CARRY | L4_TOO_LARGE | L4_TOO_LARGE_1000),
	(uint8_t)(L4_CARRY | L4_TOO_LARGE | L4_TOO_LARGE_1000),
	(uint8_t)(L4_CARRY | L4_TOO_LARGE | L4_TOO_LARGE_1000),
	(uint8_t)(L4_CARRY | L4_TOO_LARGE | L4_TOO_LARGE_1000),
	(uint8_t)(L4_CARRY | L4_TOO_LARGE | L4_TOO_LARGE_1000),
	(uint8_t)(L4_CARRY | L4_TOO_LARGE | L4_TOO_LARGE_1000),
	(uint8_t)(L4_CARRY | L4_TOO_LARGE | L4_TOO_LARGE_1000 | L4_SURROGATE),
	(uint8_t)(L4_CARRY | L4_TOO_LARGE | L4_TOO_LARGE_1000),
	(uint8_t)(L4_CARRY | L4_TOO_LARGE | L4_TOO_LARGE_1000),
};

static const uint8_t lookup4_byte_2_high[16] = {
	L4_TOO_SHORT, L4_TOO_SHORT, L4_TOO_SHORT, L4_TOO_SHORT,
	L4_TOO_SHORT, L4_TOO_SHORT, L4_TOO_SHORT, L4_TOO_SHORT,
	(uint8_t)(L4_TOO_LONG | L4_OVERLONG_2 | L4_TWO_CONTS | L4_OVERLONG_3 | L4_TOO_LARGE_1000 | L4_OVERLONG_4),
	(uint8_t)(L4_TOO_LONG | L4_OVERLONG_2 | L4_TWO_CONTS | L4_OVERLONG_3 | L4_TOO_LARGE),
	(uint8_t)(L4_TOO_LONG | L4_OVERLONG_2 | L4_TWO_CONTS | L4_SURROGATE | L4_TOO_LARGE),
	(uint8_t)(L4_TOO_LONG | L4_OVERLONG_2 | L4_TWO_CONTS | L4_SURROGATE | L4_TOO_LARGE),
	L4_TOO_SHORT, L4_TOO_SHORT, L4_TOO_SHORT, L4_TOO_SHORT,
};

UTF8_BENCH_INLINE uint8_t
lookup4_check_special_cases(uint8_t prev1, uint8_t cur)
{
	return (uint8_t)(lookup4_byte_1_high[prev1 >> 4] & lookup4_byte_1_low[prev1 & 0x0Fu] &
			 lookup4_byte_2_high[cur >> 4]);
}

UTF8_BENCH_INLINE uint8_t
lookup4_u8_sat_sub(uint8_t a, uint8_t b)
{
	return a > b ? (uint8_t)(a - b) : 0;
}

UTF8_BENCH_INLINE uint8_t
lookup4_must_be_2_3_continuation(uint8_t prev2, uint8_t prev3)
{
	/* Experiments / simdjson: saturating_sub(0xe0-0x80), saturating_sub(0xf0-0x80) */
	uint8_t is_third  = lookup4_u8_sat_sub(prev2, (uint8_t)(0xE0u - 0x80u));
	uint8_t is_fourth = lookup4_u8_sat_sub(prev3, (uint8_t)(0xF0u - 0x80u));

	return (uint8_t)(is_third | is_fourth);
}

UTF8_BENCH_INLINE uint8_t
lookup4_byte_before(const uint8_t cur[16], const uint8_t prev[16], unsigned j, unsigned k)
{
	if (j >= k) {
		return cur[j - k];
	}

	return prev[16u + j - k];
}

UTF8_BENCH_INLINE void
lookup4_check_utf8_bytes_16(const uint8_t cur[16], const uint8_t prev_in[16], int *err)
{
	for (unsigned j = 0; j < 16u; j++) {
		uint8_t p1 = lookup4_byte_before(cur, prev_in, j, 1);
		uint8_t p2 = lookup4_byte_before(cur, prev_in, j, 2);
		uint8_t p3 = lookup4_byte_before(cur, prev_in, j, 3);
		uint8_t c  = cur[j];
		uint8_t sc = lookup4_check_special_cases(p1, c);
		uint8_t mb = lookup4_must_be_2_3_continuation(p2, p3);
		uint8_t x  = (uint8_t)((mb & 0x80u) ^ sc);

		if (x != 0) {
			*err = 1;
		}
	}
}

UTF8_BENCH_INLINE int
lookup4_is_incomplete_16(const uint8_t chunk[16])
{
	static const uint8_t max_array[32] = {
		255, 255, 255, 255, 255, 255, 255, 255,
		255, 255, 255, 255, 255, 255, 255, 255,
		255, 255, 255, 255, 255, 255, 255, 255,
		255, 255, 255, 255, 255, (uint8_t)(0xF0u - 1u), (uint8_t)(0xE0u - 1u), (uint8_t)(0xC0u - 1u),
	};
	int inc = 0;

	for (unsigned j = 0; j < 16u; j++) {
		if (chunk[j] > max_array[16u + j]) {
			inc = 1;
		}
	}
	return inc;
}

UTF8_BENCH_INLINE int
lookup4_block_all_ascii_64(const uint8_t b[64])
{
	for (unsigned i = 0; i < 64u; i++) {
		if (b[i] >= 0x80u) {
			return 0;
		}
	}

	return 1;
}

UTF8_BENCH_INLINE void
lookup4_check_next_input_64(const uint8_t block[64], uint8_t prev[16], int *prev_inc,
			    int *err)
{
	if (lookup4_block_all_ascii_64(block)) {
		if (*prev_inc) {
			*err = 1;
		}

		return;
	}

	lookup4_check_utf8_bytes_16(block + 0, prev, err);
	lookup4_check_utf8_bytes_16(block + 16, block + 0, err);
	lookup4_check_utf8_bytes_16(block + 32, block + 16, err);
	lookup4_check_utf8_bytes_16(block + 48, block + 32, err);

	*prev_inc = lookup4_is_incomplete_16(block + 48);
	memcpy(prev, block + 48, 16);
}

UTF8_BENCH_INLINE bool
utf8_slow_lookup4_validate(const uint8_t *buf, size_t buf_sz)
{
	uint8_t prev[16] = { 0 };
	int	prev_inc = 0;
	int	err = 0;
	size_t	i = 0;

	while (i + 64 <= buf_sz) {
		lookup4_check_next_input_64(buf + i, prev, &prev_inc, &err);

		if (err) {
			return false;
		}

		i += 64;
	}

	/* validator.h: padded remainder only if length % 64 != 0 */
	if (buf_sz > i) {
		uint8_t tail[64] = { 0 };

		memcpy(tail, buf + i, buf_sz - i);
		lookup4_check_next_input_64(tail, prev, &prev_inc, &err);

		if (err) {
			return false;
		}
	}

	/* check_eof(): incomplete multibyte cannot end here */
	if (prev_inc) {
		return false;
	}

	return true;
}
