// UTF8_BENCH_USE_ALWAYS_INLINE: always_inline for utf8_slow_* in this TU.
#define UTF8_BENCH_USE_ALWAYS_INLINE
#include "utf8_slow_scalar.h"

bool
bench_utf8_Sscalar(const uint8_t* buf, size_t buf_sz)
{
	return utf8_slow_scalar(buf, buf_sz);
}
