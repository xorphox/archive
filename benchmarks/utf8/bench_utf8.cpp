#include <benchmark/benchmark.h>

#include <cstddef>
#include <cstdint>
#include <random>
#include <vector>

extern "C" {
bool bench_utf8_scalar_EeR(const uint8_t* buf, size_t buf_sz);
bool bench_utf8_scalar_EeC(const uint8_t* buf, size_t buf_sz);
bool bench_utf8_eec_orig(const uint8_t* buf, size_t buf_sz);
bool bench_utf8_Forig_Sscalar_R(const uint8_t* buf, size_t buf_sz);
bool bench_utf8_Fmemcpy_Sscalar_R(const uint8_t* buf, size_t buf_sz);
bool bench_utf8_Fsingle_Sscalar_R(const uint8_t* buf, size_t buf_sz);
bool bench_utf8_Sscalar(const uint8_t* buf, size_t buf_sz);
bool bench_utf8_Fsse2_Sscalar_EeR(const uint8_t* buf, size_t buf_sz);
bool bench_utf8_Fsse2_Sscalar_EeC(const uint8_t* buf, size_t buf_sz);
bool bench_utf8_Fscalar_Sutf8d_EeC(const uint8_t* buf, size_t buf_sz);
#if defined(__x86_64__)
bool bench_utf8_scalar_EeC_haswell(const uint8_t* buf, size_t buf_sz);
bool bench_utf8_Fsse41_Sscalar_EeC(const uint8_t* buf, size_t buf_sz);
bool bench_utf8_Favx2_Sscalar_EeC(const uint8_t* buf, size_t buf_sz);
bool bench_utf8_Favx2_Slookup4_EeC(const uint8_t* buf, size_t buf_sz);
bool bench_utf8_Favx2_Sutf8d_EeC(const uint8_t* buf, size_t buf_sz);
#endif
}

namespace {
// DO NOT REMOVE THE #define LINE BELOW FROM THIS FILE — only toggle by commenting/uncommenting it.
#define BENCH_UTF8_FULL_RANGE
// Temp: single 8 MiB only when the #define above is commented out (re-enable for 8 B … 8 MiB sweep).
#ifndef BENCH_UTF8_FULL_RANGE
#define BENCH_UTF8_SIZE_SUFFIX ->Arg(8 << 20)
#else
#define BENCH_UTF8_SIZE_SUFFIX ->RangeMultiplier(4)->Range(8, 8 << 20)
#endif
// Full-range caveat: with BENCH_UTF8_FULL_RANGE, a full ./bench_utf8 run executes smaller sizes
// (and other suites) before …/8388608, so turbo/thermal/cache differ from a single-Arg build.
// Compare fairly with --benchmark_filter=BM_scalar_EeC_ascii_random/8388608, or CMake
// -DUTF8_BENCH_FULL_RANGE=ON without editing this file.
// Input size for benchmarks (state.range(0)): default single 8 MiB; full sweep also via CMake
// -DUTF8_BENCH_FULL_RANGE=ON.

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

static void
AppendUtf8Typical(std::vector<uint8_t>& buf, size_t target_size, std::mt19937& rng)
{
	std::uniform_int_distribution<uint32_t> d_ascii(0x20, 0x7E);
	std::uniform_int_distribution<uint32_t> d_lat(0xA0, 0xFF);
	std::uniform_int_distribution<uint32_t> d_greek(0x370, 0x3FF);
	std::uniform_int_distribution<uint32_t> d_cjk(0x4E00, 0x9FFF);
	std::discrete_distribution<int> kind({ 40, 15, 15, 30 });

	while (buf.size() < target_size) {
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
		if (buf.size() + tmp.size() > target_size) {
			while (buf.size() < target_size) {
				buf.push_back(static_cast<uint8_t>(' '));
			}
			break;
		}
		buf.insert(buf.end(), tmp.begin(), tmp.end());
	}
}

// First half random ASCII, second half multibyte-heavy (AppendUtf8Typical).
void
FillHalfAsciiFirst(std::vector<uint8_t>& buf, std::mt19937& rng)
{
	const size_t n = buf.size();
	const size_t na = n / 2;
	buf.clear();
	buf.reserve(n);
	std::uniform_int_distribution<unsigned> d(0, 0x7F);
	for (size_t i = 0; i < na; i++) {
		buf.push_back(static_cast<uint8_t>(d(rng)));
	}
	AppendUtf8Typical(buf, n, rng);
}

// Full buffer: same multibyte-heavy mix as AppendUtf8Typical (slow path / scalar heavy).
void
FillUtf8Typical(std::vector<uint8_t>& buf, std::mt19937& rng)
{
	const size_t n = buf.size();
	buf.clear();
	buf.reserve(n);
	AppendUtf8Typical(buf, n, rng);
}

// Long runs of printable ASCII (typed text), then occasionally one emoji
// (supplementary plane → 4-byte UTF-8). Matches “English chat + rare 😀”
// better than sprinkling 3-byte CJK every few bytes.
void
FillMostlyAscii(std::vector<uint8_t>& buf, std::mt19937& rng)
{
	const size_t n = buf.size();
	std::uniform_int_distribution<int> ascii_ch(0x20, 0x7E);
	std::uniform_int_distribution<size_t> ascii_run_len(64, 512);
	std::uniform_int_distribution<uint32_t> emoji_cp(0x1F600u, 0x1F64Fu);

	size_t i = 0;
	while (i < n) {
		const size_t rem = n - i;
		if (rem < 4) {
			while (i < n) {
				buf[i++] = static_cast<uint8_t>(ascii_ch(rng));
			}
			break;
		}

		const size_t run = std::min(rem, ascii_run_len(rng));
		for (size_t j = 0; j < run; j++) {
			buf[i++] = static_cast<uint8_t>(ascii_ch(rng));
		}

		if (i >= n) {
			break;
		}

		if (n - i < 4) {
			while (i < n) {
				buf[i++] = static_cast<uint8_t>(ascii_ch(rng));
			}
			break;
		}

		const uint32_t cp = emoji_cp(rng);
		buf[i] = static_cast<uint8_t>(0xF0u | (cp >> 18));
		buf[i + 1] = static_cast<uint8_t>(0x80u | ((cp >> 12) & 0x3Fu));
		buf[i + 2] = static_cast<uint8_t>(0x80u | ((cp >> 6) & 0x3Fu));
		buf[i + 3] = static_cast<uint8_t>(0x80u | (cp & 0x3Fu));
		i += 4;
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
	BENCHMARK(BM_##impl_name##_ascii_random) BENCH_UTF8_SIZE_SUFFIX;				\
	static void BM_##impl_name##_half_ascii_first(benchmark::State& st)		   \
	{																		   \
		RunBench(st, fn, FillHalfAsciiFirst);									   \
	}																		   \
	BENCHMARK(BM_##impl_name##_half_ascii_first) BENCH_UTF8_SIZE_SUFFIX;		   \
	static void BM_##impl_name##_utf8_typical(benchmark::State& st)			   \
	{																		   \
		RunBench(st, fn, FillUtf8Typical);									   \
	}																		   \
	BENCHMARK(BM_##impl_name##_utf8_typical) BENCH_UTF8_SIZE_SUFFIX;			   \
	static void BM_##impl_name##_mostly_ascii(benchmark::State& st)			   \
	{																		   \
		RunBench(st, fn, FillMostlyAscii);										 \
	}																		   \
	BENCHMARK(BM_##impl_name##_mostly_ascii) BENCH_UTF8_SIZE_SUFFIX

// Legacy table column → BM_<impl>_…:
//   early_exit_cont      → scalar_EeC  (eec_orig: same algo, single-TU; registered just above scalar_EeC)
//   fast_simd_cont       → Fsse2_Sscalar_EeC
//   fast_simd4_cont      → Fsse41_Sscalar_EeC   (x86_64 Haswell object TU)
//   fast_avx2_cont       → Favx2_Sscalar_EeC    (x86_64 Haswell object TU)
//   fast_avx2_lookup4    → Favx2_Slookup4_EeC   (x86_64 Haswell object TU)
//   fast_avx2_utf8d      → Favx2_Sutf8d_EeC     (x86_64 Haswell object TU)
//   fast_scalar_utf8d    → Fscalar_Sutf8d_EeC
//   scalar_slow          → Sscalar
// BENCH_SUITE(scalar_EeR, bench_utf8_scalar_EeR);
BENCH_SUITE(Fsse2_Sscalar_EeC, bench_utf8_Fsse2_Sscalar_EeC);
BENCH_SUITE(Fscalar_Sutf8d_EeC, bench_utf8_Fscalar_Sutf8d_EeC);
BENCH_SUITE(Sscalar, bench_utf8_Sscalar);
#if defined(__x86_64__)
BENCH_SUITE(Fsse41_Sscalar_EeC, bench_utf8_Fsse41_Sscalar_EeC);
BENCH_SUITE(Favx2_Sscalar_EeC, bench_utf8_Favx2_Sscalar_EeC);
BENCH_SUITE(Favx2_Slookup4_EeC, bench_utf8_Favx2_Slookup4_EeC);
BENCH_SUITE(Favx2_Sutf8d_EeC, bench_utf8_Favx2_Sutf8d_EeC);
#endif
// eec_orig then scalar_EeC: both after SIMD/AVX suites; full-range A/B (single-TU vs headers).
// scalar_EeC alone was last for thermal-order experiments; paired here for fairer comparison.
BENCH_SUITE(eec_orig, bench_utf8_eec_orig);
BENCH_SUITE(scalar_EeC, bench_utf8_scalar_EeC);
} // namespace

BENCHMARK_MAIN();
