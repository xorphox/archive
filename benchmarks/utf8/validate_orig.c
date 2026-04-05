#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stddef.h>
#include <stddef.h>

#include "utf8_slow_scalar.h"

UTF8_BENCH_INLINE uint64_t
accumulate64(const uint8_t* buf, size_t buf_sz)
{
	// 1. HEAD: peel until 8-byte aligned
	uintptr_t addr = (uintptr_t)buf;
	size_t head = (-addr) & 7;

	if (head > buf_sz) {
		head = buf_sz;
	}

	uint64_t acc = 0;
	for (size_t i = 0; i < head; i++) {
		acc |= (uint64_t)buf[i];
	}

	buf += head;
	buf_sz -= head;

	// 2. BULK: aligned 64-bit loop
	size_t n64 = buf_sz / 8;
	const uint64_t* p64 = (const uint64_t*)buf;
	for (size_t i = 0; i < n64; i++) {
		acc |= p64[i];
	}

	// 3. TAIL: last 0–7 bytes
	size_t tail = buf_sz & 7;
	if (tail) {
		const uint8_t* t = buf + (n64 * 8);
		for (size_t i = 0; i < tail; i++) {
			acc |= t[i];
		}
	}

	return acc;
}

bool
as_str_is_valid_utf8(const uint8_t* buf, size_t buf_sz)
{
	if (buf == NULL) {
		return buf_sz == 0;
	}

	if ((accumulate64(buf, buf_sz) & 0x8080808080808080ULL) == 0) {
		return true;
	}

	return utf8_slow_scalar_validate(buf, buf_sz);
}
