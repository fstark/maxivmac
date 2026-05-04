#include "storage/appledouble.h"
#include "util/macroman.h"

#include <fstream>
#include <string>
#include <vector>

namespace appledouble
{

namespace
{

std::string ReadFileContents(const std::filesystem::path &path)
{
	std::ifstream f(path, std::ios::binary);
	return {std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>()};
}

} // anonymous namespace

std::vector<uint8_t> MacRomanFromUTF8File(const std::filesystem::path &hostPath)
{
	auto mr = ::MacRomanFromUTF8(ReadFileContents(hostPath));
	return {reinterpret_cast<const uint8_t *>(mr.data()),
			reinterpret_cast<const uint8_t *>(mr.data()) + mr.size()};
}

uint32_t MacRomanSizeFromUTF8File(const std::filesystem::path &hostPath)
{
	return static_cast<uint32_t>(::MacRomanFromUTF8(ReadFileContents(hostPath)).size());
}

void UTF8FileFromMacRoman(const std::filesystem::path &hostPath, std::span<const uint8_t> macRoman)
{
	auto utf8 = ::UTF8FromMacRoman(macRoman);
	std::ofstream out(hostPath, std::ios::binary | std::ios::trunc);
	out.write(utf8.data(), static_cast<std::streamsize>(utf8.size()));
}

} // namespace appledouble
