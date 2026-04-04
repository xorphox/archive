#ifndef UTF8_SCALAR_SLOW_H
#define UTF8_SCALAR_SLOW_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Shared strict UTF-8 scalar validator (multibyte + overlong + surrogates). */
bool utf8_validate_scalar_slow(const uint8_t* buf, size_t buf_sz);

#endif
