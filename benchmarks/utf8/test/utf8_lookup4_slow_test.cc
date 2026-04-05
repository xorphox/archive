/*
 * Parity vs utf8_validate_scalar_slow:
 *   - utf8_validate_lookup4_slow (custom DFA; may diverge until fixed)
 *   - utf8_validate_utf8d_slow (Höhrmann utf8d scalar)
 */
#include <gtest/gtest.h>

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "utf8_scalar_slow.h"
#include "utf8_validate_lookup4_slow.h"
#include "utf8_validate_utf8d_slow.h"

namespace {

bool
Scalar(std::string_view s)
{
	return utf8_validate_scalar_slow(
		reinterpret_cast<const uint8_t*>(s.data()), s.size());
}

bool
Lookup4(std::string_view s)
{
	return utf8_validate_lookup4_slow(
		reinterpret_cast<const uint8_t*>(s.data()), s.size());
}

bool
Utf8dSlow(std::string_view s)
{
	return utf8_validate_utf8d_slow(
		reinterpret_cast<const uint8_t*>(s.data()), s.size());
}

template <typename Fn>
void
ExpectVsScalar(std::string_view bytes, const char* note, Fn fn, const char* tag)
{
	const bool sc = Scalar(bytes);
	const bool got = fn(bytes);
	EXPECT_EQ(got, sc) << note << " | scalar=" << sc << " " << tag << "=" << got;
}

void
ExpectLookup4Agree(std::string_view bytes, const char* note)
{
	ExpectVsScalar(bytes, note, Lookup4, "lookup4");
}

void
ExpectUtf8dAgree(std::string_view bytes, const char* note)
{
	ExpectVsScalar(bytes, note, Utf8dSlow, "utf8d");
}

std::string
U8(uint32_t cp)
{
	std::string out;
	if (cp <= 0x7Fu) {
		out.push_back(static_cast<char>(cp));
	}
	else if (cp <= 0x7FFu) {
		out.push_back(static_cast<char>(0xC0u | (cp >> 6)));
		out.push_back(static_cast<char>(0x80u | (cp & 0x3Fu)));
	}
	else if (cp <= 0xFFFFu) {
		out.push_back(static_cast<char>(0xE0u | (cp >> 12)));
		out.push_back(static_cast<char>(0x80u | ((cp >> 6) & 0x3Fu)));
		out.push_back(static_cast<char>(0x80u | (cp & 0x3Fu)));
	}
	else {
		out.push_back(static_cast<char>(0xF0u | (cp >> 18)));
		out.push_back(static_cast<char>(0x80u | ((cp >> 12) & 0x3Fu)));
		out.push_back(static_cast<char>(0x80u | ((cp >> 6) & 0x3Fu)));
		out.push_back(static_cast<char>(0x80u | (cp & 0x3Fu)));
	}
	return out;
}

} // namespace

TEST(Utf8Lookup4VsScalar, Empty)
{
	ExpectLookup4Agree("", "empty");
}

TEST(Utf8Lookup4VsScalar, Ascii)
{
	ExpectLookup4Agree("hello", "ascii");
	ExpectLookup4Agree(std::string(256, 'A'), "long ascii");
}

TEST(Utf8Lookup4VsScalar, ValidBoundaries)
{
	const std::vector<uint32_t> cps = {
		0x24,
		0x7F,
		0x80,
		0x7FF,
		0x800,
		0xFFF,
		0x1000,
		0xD7FF,
		0xE000,
		0xFFFF,
		0x10000,
		0x10FFFF,
	};
	for (uint32_t cp : cps) {
		const std::string s = U8(cp);
		ExpectLookup4Agree(s, ("valid U+" + std::to_string(cp)).c_str());
	}
}

TEST(Utf8Lookup4VsScalar, ValidConcatenated)
{
	std::string s;
	for (uint32_t cp : { 0x41u, 0x80u, 0x800u, 0x10000u, 0x10FFFFu }) {
		s += U8(cp);
	}
	ExpectLookup4Agree(s, "concat valid sequences");
}

TEST(Utf8Lookup4VsScalar, InvalidSamples)
{
	const std::vector<std::pair<std::string, const char*>> bad = {
		{ "\x80", "lone continuation" },
		{ "\xBF", "lone continuation" },
		{ "\xC0\xAF", "overlong 2-byte" },
		{ "\xE0\x80\xAF", "overlong 3-byte" },
		{ "\xF0\x80\x80\xAF", "overlong 4-byte" },
		{ "\xE0\xA0", "truncated 3-byte" },
		{ "\xF0\x90\x80", "truncated 4-byte" },
		{ "\xC2\x00", "bad continuation" },
		{ "\xED\xA0\x80", "surrogate U+D800" },
		{ "\xED\xBF\xBF", "surrogate U+DFFF" },
		{ "\xF4\x90\x80\x80", "codepoint > U+10FFFF" },
		{ "\xF5\x80\x80\x80", "lead F5" },
		{ "\xFF", "invalid lead" },
		{ "\xFE", "invalid lead" },
	};
	for (const auto& [b, note] : bad) {
		ExpectLookup4Agree(b, note);
	}
}

TEST(Utf8Lookup4VsScalar, ExhaustiveBmpNonSurrogates)
{
	for (uint32_t cp = 0; cp <= 0xFFFFu; cp++) {
		if (cp >= 0xD800u && cp <= 0xDFFFu) {
			continue;
		}
		const std::string s = U8(cp);
		ExpectLookup4Agree(s, ("U+" + std::to_string(cp)).c_str());
	}
}

TEST(Utf8Lookup4VsScalar, ExhaustiveSupplementaryPlane)
{
	for (uint32_t cp = 0x10000u; cp <= 0x10FFFFu; cp++) {
		const std::string s = U8(cp);
		ExpectLookup4Agree(s, ("U+" + std::to_string(cp)).c_str());
	}
}

TEST(Utf8Utf8dVsScalar, Empty)
{
	ExpectUtf8dAgree("", "empty");
}

TEST(Utf8Utf8dVsScalar, Ascii)
{
	ExpectUtf8dAgree("hello", "ascii");
	ExpectUtf8dAgree(std::string(256, 'A'), "long ascii");
}

TEST(Utf8Utf8dVsScalar, ValidBoundaries)
{
	const std::vector<uint32_t> cps = {
		0x24,
		0x7F,
		0x80,
		0x7FF,
		0x800,
		0xFFF,
		0x1000,
		0xD7FF,
		0xE000,
		0xFFFF,
		0x10000,
		0x10FFFF,
	};
	for (uint32_t cp : cps) {
		const std::string s = U8(cp);
		ExpectUtf8dAgree(s, ("valid U+" + std::to_string(cp)).c_str());
	}
}

TEST(Utf8Utf8dVsScalar, ValidConcatenated)
{
	std::string s;
	for (uint32_t cp : { 0x41u, 0x80u, 0x800u, 0x10000u, 0x10FFFFu }) {
		s += U8(cp);
	}
	ExpectUtf8dAgree(s, "concat valid sequences");
}

TEST(Utf8Utf8dVsScalar, InvalidSamples)
{
	const std::vector<std::pair<std::string, const char*>> bad = {
		{ "\x80", "lone continuation" },
		{ "\xBF", "lone continuation" },
		{ "\xC0\xAF", "overlong 2-byte" },
		{ "\xE0\x80\xAF", "overlong 3-byte" },
		{ "\xF0\x80\x80\xAF", "overlong 4-byte" },
		{ "\xE0\xA0", "truncated 3-byte" },
		{ "\xF0\x90\x80", "truncated 4-byte" },
		{ "\xC2\x00", "bad continuation" },
		{ "\xED\xA0\x80", "surrogate U+D800" },
		{ "\xED\xBF\xBF", "surrogate U+DFFF" },
		{ "\xF4\x90\x80\x80", "codepoint > U+10FFFF" },
		{ "\xF5\x80\x80\x80", "lead F5" },
		{ "\xFF", "invalid lead" },
		{ "\xFE", "invalid lead" },
	};
	for (const auto& [b, note] : bad) {
		ExpectUtf8dAgree(b, note);
	}
}

TEST(Utf8Utf8dVsScalar, ExhaustiveBmpNonSurrogates)
{
	for (uint32_t cp = 0; cp <= 0xFFFFu; cp++) {
		if (cp >= 0xD800u && cp <= 0xDFFFu) {
			continue;
		}
		const std::string s = U8(cp);
		ExpectUtf8dAgree(s, ("U+" + std::to_string(cp)).c_str());
	}
}

TEST(Utf8Utf8dVsScalar, ExhaustiveSupplementaryPlane)
{
	for (uint32_t cp = 0x10000u; cp <= 0x10FFFFu; cp++) {
		const std::string s = U8(cp);
		ExpectUtf8dAgree(s, ("U+" + std::to_string(cp)).c_str());
	}
}
