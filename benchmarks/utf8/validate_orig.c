#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stddef.h>
#include <stddef.h>

static inline uint64_t
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

	static const uint32_t cp_min[] = { 0, 0x80, 0x800, 0x10000 };

	// Slow path: validate multi-byte sequences.
	for (size_t i = 0; i < buf_sz; i++) {
		uint8_t b = buf[i];

		if (b <= 0x7F) {
			continue;
		}

		uint32_t cp;
		uint32_t n;

		if ((b & 0xE0) == 0xC0) {
			cp = b & 0x1F;
			n = 1;
		}
		else if ((b & 0xF0) == 0xE0) {
			cp = b & 0x0F;
			n = 2;
		}
		else if ((b & 0xF8) == 0xF0) {
			cp = b & 0x07;
			n = 3;
		}
		else {
			return false;
		}

		if (i + n >= buf_sz) {
			return false;
		}

		for (uint32_t j = 0; j < n; j++) {
			uint8_t c = buf[++i];

			if ((c & 0xC0) != 0x80) {
				return false;
			}

			cp = (cp << 6) | (c & 0x3F);
		}

		if (cp < cp_min[n] || cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF)) {
			return false;
		}
	}

	return true;
}
