#pragma once

/*
 * Höhrmann utf8d DFA (scalar): 256 class bytes + 144 transition bytes.
 * http://bjoern.hoehrmann.de/utf-8/decoder/dfa/
 *
 * Copyright (c) 2008-2009 Bjoern Hoehrmann — table per terms on the page above.
 * Not the same as simdjson/Lemire lookup4; kept here for comparison.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "utf8_bench_inline.h"

#define UTF8_UTF8D_ACCEPT 0u
#define UTF8_UTF8D_REJECT 1u

static const uint8_t utf8_validate_utf8d_slow_table[] = {
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,
	7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
	8,8,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
	10,3,3,3,3,3,3,3,3,3,3,3,3,4,3,3,
	11,6,6,6,5,8,8,8,8,8,8,8,8,8,8,8,
	0,1,2,3,5,8,7,1,1,1,4,6,1,1,1,1,
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,1,1,1,1,1,0,1,0,1,1,1,1,1,1,
	1,2,1,1,1,1,1,2,1,2,1,1,1,1,1,1,1,1,1,1,1,1,1,2,1,1,1,1,1,1,1,1,
	1,2,1,1,1,1,1,1,1,2,1,1,1,1,1,1,1,1,1,1,1,1,1,3,1,3,1,1,1,1,1,1,
	1,3,1,1,1,1,1,3,1,3,1,1,1,1,1,1,1,3,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
};

UTF8_BENCH_INLINE bool
utf8_validate_utf8d_slow(const uint8_t *buf, size_t buf_sz)
{
	uint32_t state = UTF8_UTF8D_ACCEPT;

	for (size_t i = 0; i < buf_sz; i++) {
		uint32_t type = utf8_validate_utf8d_slow_table[buf[i]];

		state = utf8_validate_utf8d_slow_table[256 + state * 16 + type];
		if (state == UTF8_UTF8D_REJECT) {
			return false;
		}
	}

	return state == UTF8_UTF8D_ACCEPT;
}
