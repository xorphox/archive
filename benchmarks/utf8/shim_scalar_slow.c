#include "utf8_scalar_slow.h"

bool
bench_utf8_scalar_slow(const uint8_t* buf, size_t buf_sz)
{
	return utf8_validate_scalar_slow(buf, buf_sz);
}
