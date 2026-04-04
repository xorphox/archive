#include <benchmark/benchmark.h>

#include <cstddef>
#include <cstdint>
#include <random>
#include <vector>

extern "C" {
bool bench_utf8_early_exit(const uint8_t* buf, size_t buf_sz);
bool bench_utf8_early_exit_continue(const uint8_t* buf, size_t buf_sz);
bool bench_utf8_orig(const uint8_t* buf, size_t buf_sz);
bool bench_utf8_pre_memcpy(const uint8_t* buf, size_t buf_sz);
bool bench_utf8_pre_single(const uint8_t* buf, size_t buf_sz);
bool bench_utf8_scalar_slow(const uint8_t* buf, size_t buf_sz);
bool bench_utf8_fast_simd_restart(const uint8_t* buf, size_t buf_sz);
bool bench_utf8_fast_simd_continue(const uint8_t* buf, size_t buf_sz);
bool bench_utf8_all_simd(const uint8_t* buf, size_t buf_sz);
}

namespace {

void
Utf8Append(uint32_t cp, std::vector<uint8_t>& out)
{
	if (cp <= 0x7Fu) {
		out.push_back(static_cast<uint8_t>(cp));
	}
	else if (cp <= 0x7FFu) {
		out.push_back(static_cast<uint8_t>(0xC0u | (cp >> 6)));
		out.push_back(static_cast<uint8_t>(0x80u | (cp & 0x3Fu)));
	}
	else if (cp <= 0xFFFFu) {
		out.push_back(static_cast<uint8_t>(0xE0u | (cp >> 12)));
		out.push_back(static_cast<uint8_t>(0x80u | ((cp >> 6) & 0x3Fu)));
		out.push_back(static_cast<uint8_t>(0x80u | (cp & 0x3Fu)));
	}
	else {
		out.push_back(static_cast<uint8_t>(0xF0u | (cp >> 18)));
		out.push_back(static_cast<uint8_t>(0x80u | ((cp >> 12) & 0x3Fu)));
		out.push_back(static_cast<uint8_t>(0x80u | ((cp >> 6) & 0x3Fu)));
		out.push_back(static_cast<uint8_t>(0x80u | (cp & 0x3Fu)));
	}
}

void
FillAsciiRandom(std::vector<uint8_t>& buf, std::mt19937& rng)
{
	std::uniform_int_distribution<unsigned> d(0, 0x7F);
	for (auto& b : buf) {
		b = static_cast<uint8_t>(d(rng));
	}
}

void
FillUtf8Typical(std::vector<uint8_t>& buf, std::mt19937& rng)
{
	const size_t n = buf.size();
	buf.clear();
	buf.reserve(n);

	std::uniform_int_distribution<uint32_t> d_ascii(0x20, 0x7E);
	std::uniform_int_distribution<uint32_t> d_lat(0xA0, 0xFF);
	std::uniform_int_distribution<uint32_t> d_greek(0x370, 0x3FF);
	std::uniform_int_distribution<uint32_t> d_cjk(0x4E00, 0x9FFF);
	std::discrete_distribution<int> kind({ 40, 15, 15, 30 });

	while (buf.size() < n) {
		uint32_t cp = 0x20;
		switch (kind(rng)) {
		case 0:
			cp = d_ascii(rng);
			break;
		case 1:
			cp = d_lat(rng);
			break;
		case 2:
			cp = d_greek(rng);
			break;
		default:
			cp = d_cjk(rng);
			break;
		}

		std::vector<uint8_t> tmp;
		tmp.reserve(4);
		Utf8Append(cp, tmp);
		if (buf.size() + tmp.size() > n) {
			while (buf.size() < n) {
				buf.push_back(static_cast<uint8_t>(' '));
			}
			break;
		}
		buf.insert(buf.end(), tmp.begin(), tmp.end());
	}
}

void
FillMostlyAscii(std::vector<uint8_t>& buf, std::mt19937& rng)
{
	std::uniform_int_distribution<int> ascii(0x20, 0x7E);
	for (auto& b : buf) {
		b = static_cast<uint8_t>(ascii(rng));
	}

	std::bernoulli_distribution place_utf8(0.03);
	std::uniform_int_distribution<uint32_t> d_cjk(0x4E00, 0x9FFF);

	for (size_t i = 0; i + 3 <= buf.size();) {
		if (place_utf8(rng)) {
			const uint32_t cp = d_cjk(rng);
			buf[i] = static_cast<uint8_t>(0xE0u | ((cp >> 12) & 0x0Fu));
			buf[i + 1] = static_cast<uint8_t>(0x80u | ((cp >> 6) & 0x3Fu));
			buf[i + 2] = static_cast<uint8_t>(0x80u | (cp & 0x3Fu));
			i += 3;
		}
		else {
			buf[i] = static_cast<uint8_t>(ascii(rng));
			i += 1;
		}
	}
}

using FillFn = void (*)(std::vector<uint8_t>&, std::mt19937&);
using ValidateFn = bool (*)(const uint8_t*, size_t);

void
RunBench(benchmark::State& state, ValidateFn fn, FillFn fill)
{
	std::mt19937 rng(42);
	const size_t n = static_cast<size_t>(state.range(0));
	std::vector<uint8_t> buf(n);
	fill(buf, rng);

	for (auto _ : state) {
		bool ok = fn(buf.data(), buf.size());
		benchmark::DoNotOptimize(&ok);
	}

	state.SetBytesProcessed(static_cast<int64_t>(state.iterations()) *
				static_cast<int64_t>(n));
}

#define BENCH_SUITE(impl_name, fn)											   \
	static void BM_##impl_name##_ascii_random(benchmark::State& st)				 \
	{																		   \
		RunBench(st, fn, FillAsciiRandom);										 \
	}																		   \
	BENCHMARK(BM_##impl_name##_ascii_random)								   \
		->RangeMultiplier(2)												   \
		->Range(8, 8 << 20);													\
	static void BM_##impl_name##_utf8_typical(benchmark::State& st)			   \
	{																		   \
		RunBench(st, fn, FillUtf8Typical);										 \
	}																		   \
	BENCHMARK(BM_##impl_name##_utf8_typical)								   \
		->RangeMultiplier(2)												   \
		->Range(8, 8 << 20);													\
	static void BM_##impl_name##_mostly_ascii(benchmark::State& st)			   \
	{																		   \
		RunBench(st, fn, FillMostlyAscii);										 \
	}																		   \
	BENCHMARK(BM_##impl_name##_mostly_ascii)								   \
		->RangeMultiplier(2)												   \
		->Range(8, 8 << 20)

BENCH_SUITE(early_exit, bench_utf8_early_exit);
BENCH_SUITE(early_exit_continue, bench_utf8_early_exit_continue);
BENCH_SUITE(orig, bench_utf8_orig);
BENCH_SUITE(pre_memcpy, bench_utf8_pre_memcpy);
BENCH_SUITE(pre_single, bench_utf8_pre_single);
BENCH_SUITE(scalar_slow, bench_utf8_scalar_slow);
BENCH_SUITE(fast_simd_restart, bench_utf8_fast_simd_restart);
BENCH_SUITE(fast_simd_continue, bench_utf8_fast_simd_continue);
BENCH_SUITE(all_simd, bench_utf8_all_simd);

} // namespace

BENCHMARK_MAIN();
