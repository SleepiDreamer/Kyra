#pragma once
// A parser for .cube LUT files based on the Adobe Cube LUT Specification 1.0

#include <charconv>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>
#include <DirectXPackedVector.h>

struct CubeLUT
{
	std::string title;
	uint32_t size = 0;
	float domainMin[3] = { 0, 0, 0 };
	float domainMax[3] = { 1, 1, 1 };
	std::vector<float> rgb;

	[[nodiscard]] std::vector<uint16_t> ToRGBA16F() const
	{
		using namespace DirectX::PackedVector;
		const size_t texels = static_cast<size_t>(size) * size * size;
		std::vector<uint16_t> out(texels * 4);
		const HALF one = XMConvertFloatToHalf(1.0f);
		for (size_t i = 0; i < texels; ++i)
		{
			out[i * 4 + 0] = XMConvertFloatToHalf(rgb[i * 3 + 0]);
			out[i * 4 + 1] = XMConvertFloatToHalf(rgb[i * 3 + 1]);
			out[i * 4 + 2] = XMConvertFloatToHalf(rgb[i * 3 + 2]);
			out[i * 4 + 3] = one;
		}
		return out;
	}
};

namespace cube_detail
{
	inline std::string_view Trim(std::string_view s)
	{
		constexpr const char* ws = " \t\r\n";
		const size_t b = s.find_first_not_of(ws);
		if (b == std::string_view::npos) return {};
		const size_t e = s.find_last_not_of(ws);
		return s.substr(b, e - b + 1);
	}

	inline bool ParseFloats(std::string_view s, float* out, int count)
	{
		const char* p = s.data();
		const char* end = p + s.size();
		for (int i = 0; i < count; ++i)
		{
			while (p < end && (*p == ' ' || *p == '\t')) ++p;
			if (p < end && *p == '+') ++p;
			auto [ptr, ec] = std::from_chars(p, end, out[i]);
			if (ec != std::errc()) return false;
			p = ptr;
		}
		while (p < end && (*p == ' ' || *p == '\t')) ++p;
		return p == end;
	}
}

inline bool ParseCubeLUT(const std::filesystem::path& path, CubeLUT& lut, std::string* error = nullptr)
{
	using namespace cube_detail;

	size_t lineNo = 0;
	auto fail = [&](const std::string& msg)
		{
			if (error) *error = path.string() + ":" + std::to_string(lineNo) + ": " + msg;
			return false;
		};

	std::ifstream file(path);
	if (!file) return fail("could not open file");

	lut = CubeLUT{};
	size_t expectedFloats = 0;
	std::string raw;

	while (std::getline(file, raw))
	{
		++lineNo;
		std::string_view line = Trim(raw);
		if (line.empty() || line[0] == '#') continue;

		// Data line
		const char c0 = line[0];
		if ((c0 >= '0' && c0 <= '9') || c0 == '-' || c0 == '+' || c0 == '.')
		{
			if (lut.size == 0)                    return fail("data before LUT_3D_SIZE");
			if (lut.rgb.size() >= expectedFloats) return fail("more entries than LUT_3D_SIZE^3");

			float v[3];
			if (!ParseFloats(line, v, 3)) return fail("expected three floats");
			lut.rgb.insert(lut.rgb.end(), v, v + 3);
			continue;
		}

		// Keyword line
		const size_t sp = line.find_first_of(" \t");
		std::string_view key = line.substr(0, sp);
		std::string_view args = (sp == std::string_view::npos) ? std::string_view{} : Trim(line.substr(sp));

		if (key == "TITLE")
		{
			if (args.size() >= 2 && args.front() == '"' && args.back() == '"')
				args = args.substr(1, args.size() - 2);
			lut.title = std::string(args);
		}
		else if (key == "LUT_3D_SIZE")
		{
			if (lut.size != 0) return fail("duplicate LUT_3D_SIZE");
			uint32_t n = 0;
			auto [ptr, ec] = std::from_chars(args.data(), args.data() + args.size(), n);
			if (ec != std::errc() || ptr != args.data() + args.size() || n < 2 || n > 256)
				return fail("invalid LUT_3D_SIZE");
			lut.size = n;
			expectedFloats = static_cast<size_t>(n) * n * n * 3;
			lut.rgb.reserve(expectedFloats);
		}
		else if (key == "LUT_1D_SIZE")
		{
			return fail("1D LUTs / shaper LUTs are not supported");
		}
		else if (key == "DOMAIN_MIN")
		{
			if (!ParseFloats(args, lut.domainMin, 3)) return fail("invalid DOMAIN_MIN");
		}
		else if (key == "DOMAIN_MAX")
		{
			if (!ParseFloats(args, lut.domainMax, 3)) return fail("invalid DOMAIN_MAX");
		}
		else if (key == "LUT_3D_INPUT_RANGE")
		{
			float r[2];
			if (!ParseFloats(args, r, 2)) return fail("invalid LUT_3D_INPUT_RANGE");
			for (int i = 0; i < 3; ++i) { lut.domainMin[i] = r[0]; lut.domainMax[i] = r[1]; }
		}
	}

	if (lut.size == 0)
	{
		return fail("missing LUT_3D_SIZE");
	}
	if (lut.rgb.size() != expectedFloats)
	{
		return fail("expected " + std::to_string(expectedFloats / 3) +
			" entries, got " + std::to_string(lut.rgb.size() / 3));
	}
	for (int i = 0; i < 3; ++i)
	{
		if (!(lut.domainMax[i] > lut.domainMin[i])) return fail("DOMAIN_MAX must be greater than DOMAIN_MIN");
	}

	return true;
}