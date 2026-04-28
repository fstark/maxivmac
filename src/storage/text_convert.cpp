#include "storage/appledouble.h"
#include "util/macroman.h"

#include <fstream>
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
	auto content = ReadFileContents(hostPath);
	std::vector<uint8_t> result;
	result.reserve(content.size());

	size_t pos = 0;
	while (pos < content.size())
	{
		uint32_t cp = DecodeUTF8(content, pos);
		auto mr = MacRomanFromCodePoint(cp);
		result.push_back(mr.valid ? mr.byte : '?');
	}
	return result;
}

uint32_t MacRomanSizeFromUTF8File(const std::filesystem::path &hostPath)
{
	auto content = ReadFileContents(hostPath);
	uint32_t count = 0;
	size_t pos = 0;
	while (pos < content.size())
	{
		DecodeUTF8(content, pos);
		++count;
	}
	return count;
}

} // namespace appledouble
