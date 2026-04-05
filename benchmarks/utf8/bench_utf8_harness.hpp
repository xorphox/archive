#pragma once

// Shared harness for bench_utf8 (monolithic) and bench_utf8_one_* (single-impl) executables.

#include <benchmark/benchmark.h>

#include <cstddef>
#include <cstdint>
#include <random>
#include <vector>

namespace utf8_bench {

// Always register the full 8 B … 8 MiB sweep so the binary layout (and DSB
// alignment of hot loops) is identical regardless of which sizes actually run.
// Use --benchmark_filter at runtime to restrict sizes (see Makefile).
#define BENCH_UTF8_SIZE_SUFFIX ->RangeMultiplier(4)->Range(8, 8 << 20)

inline void
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

inline void
FillAsciiRandom(std::vector<uint8_t>& buf, std::mt19937& rng)
{
	std::uniform_int_distribution<unsigned> d(0, 0x7F);
	for (auto& b : buf) {
		b = static_cast<uint8_t>(d(rng));
	}
}

inline void
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

inline void
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

inline void
FillUtf8Typical(std::vector<uint8_t>& buf, std::mt19937& rng)
{
	const size_t n = buf.size();
	buf.clear();
	buf.reserve(n);
	AppendUtf8Typical(buf, n, rng);
}

inline void
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

inline void
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

#define BENCH_UTF8_SUITE(impl_name, fn)									   \
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

} // namespace utf8_bench
