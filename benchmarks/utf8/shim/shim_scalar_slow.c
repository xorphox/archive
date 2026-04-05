#include "utf8_slow_scalar.h"

bool
bench_utf8_scalar_slow(const uint8_t* buf, size_t buf_sz)
{
	return utf8_slow_scalar(buf, buf_sz);
}
