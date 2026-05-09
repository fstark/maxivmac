/*
	ad2bin — AppleDouble → MacBinary II converter

	Usage: ad2bin <file>
	Reads <file> (data fork) and ._<file> (AppleDouble sidecar).
	Writes <file>.bin in MacBinary II format.
*/

#include "storage/appledouble.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <vector>

namespace fs = std::filesystem;

/* ── CRC-16/CCITT (poly 0x1021, init 0) ──────────── */

static uint16_t crc16(const uint8_t *data, size_t len)
{
	uint16_t crc = 0;
	for (size_t i = 0; i < len; i++)
	{
		crc ^= static_cast<uint16_t>(data[i]) << 8;
		for (int b = 0; b < 8; b++)
			crc = (crc & 0x8000) ? (crc << 1) ^ 0x1021 : crc << 1;
	}
	return crc;
}

/* ── Big-endian helpers ──────────────────────────── */

static void put16(uint8_t *p, uint16_t v)
{
	p[0] = static_cast<uint8_t>(v >> 8);
	p[1] = static_cast<uint8_t>(v);
}

static void put32(uint8_t *p, uint32_t v)
{
	p[0] = static_cast<uint8_t>(v >> 24);
	p[1] = static_cast<uint8_t>(v >> 16);
	p[2] = static_cast<uint8_t>(v >> 8);
	p[3] = static_cast<uint8_t>(v);
}

/* ── Pad to 128-byte boundary ────────────────────── */

static size_t padded(size_t n) { return (n + 127) & ~size_t(127); }

/* ── Main ─────────────────────────────────────────── */

int main(int argc, char *argv[])
{
	if (argc != 2 || std::string_view(argv[1]) == "-h" || std::string_view(argv[1]) == "--help")
	{
		std::fputs("Usage: ad2bin <file>\n"
				   "Reads <file> + ._<file>, writes <file>.bin (MacBinary II)\n",
				   stderr);
		return argc == 2 ? 0 : 1;
	}

	fs::path hostPath(argv[1]);

	/* Read sidecar metadata */
	auto sidecar = appledouble::SidecarPathFor(hostPath);
	if (!fs::exists(sidecar))
	{
		std::fprintf(stderr, "ad2bin: no sidecar found at %s\n", sidecar.c_str());
		return 1;
	}

	auto fi = appledouble::GetFinderInfo(hostPath);
	uint32_t rsrcSize = appledouble::ResourceForkSize(hostPath);
	auto rsrcData = appledouble::ReadResourceFork(hostPath, 0, rsrcSize);

	/* Read data fork (may not exist — e.g. code resources) */
	std::vector<uint8_t> dataFork;
	if (fs::exists(hostPath) && fs::file_size(hostPath) > 0)
	{
		auto sz = fs::file_size(hostPath);
		dataFork.resize(static_cast<size_t>(sz));
		std::ifstream in(hostPath, std::ios::binary);
		in.read(reinterpret_cast<char *>(dataFork.data()), static_cast<std::streamsize>(sz));
	}

	/* Derive filename (truncate to 63 chars) */
	std::string name = hostPath.filename().string();
	if (name.size() > 63) name.resize(63);

	/* Build 128-byte MacBinary II header */
	std::array<uint8_t, 128> hdr{};

	/* byte 0: old version = 0 */
	/* byte 1: filename length */
	hdr[1] = static_cast<uint8_t>(name.size());
	/* bytes 2–64: filename */
	std::memcpy(&hdr[2], name.data(), name.size());

	/* bytes 65–68: type */
	put32(&hdr[65], fi.type);
	/* bytes 69–72: creator */
	put32(&hdr[69], fi.creator);
	/* byte 73: Finder flags high byte */
	hdr[73] = static_cast<uint8_t>(fi.flags >> 8);

	/* bytes 75–76: vertical position */
	put16(&hdr[75], static_cast<uint16_t>(fi.location >> 16));
	/* bytes 77–78: horizontal position */
	put16(&hdr[77], static_cast<uint16_t>(fi.location & 0xFFFF));
	/* bytes 79–80: folder ID */
	put16(&hdr[79], fi.folder);

	/* bytes 83–86: data fork length */
	put32(&hdr[83], static_cast<uint32_t>(dataFork.size()));
	/* bytes 87–90: resource fork length */
	put32(&hdr[87], static_cast<uint32_t>(rsrcData.size()));

	/* bytes 91–94: creation date (Mac epoch, already in Mac format) */
	auto fileInfo = appledouble::GetFileInfo(hostPath);
	put32(&hdr[91], fileInfo.crDate);
	/* bytes 95–98: modification date */
	put32(&hdr[95], fileInfo.modDate);

	/* byte 101: Finder flags low byte (MacBinary II) */
	hdr[101] = static_cast<uint8_t>(fi.flags & 0xFF);
	/* bytes 102–105: signature 'mBIN' */
	put32(&hdr[102], 0x6D42494E);
	/* byte 122: MacBinary II version */
	hdr[122] = 129;
	/* byte 123: minimum version to read */
	hdr[123] = 129;

	/* bytes 124–125: CRC-16 of header bytes 0–123 */
	put16(&hdr[124], crc16(hdr.data(), 124));

	/* Write output */
	fs::path outPath = hostPath;
	outPath += ".bin";

	std::ofstream out(outPath, std::ios::binary);
	if (!out)
	{
		std::fprintf(stderr, "ad2bin: cannot create %s\n", outPath.c_str());
		return 1;
	}

	out.write(reinterpret_cast<const char *>(hdr.data()), 128);

	/* Data fork + padding */
	if (!dataFork.empty())
	{
		out.write(reinterpret_cast<const char *>(dataFork.data()),
				  static_cast<std::streamsize>(dataFork.size()));
		size_t pad = padded(dataFork.size()) - dataFork.size();
		if (pad > 0)
		{
			std::array<uint8_t, 128> zeros{};
			out.write(reinterpret_cast<const char *>(zeros.data()),
					  static_cast<std::streamsize>(pad));
		}
	}

	/* Resource fork + padding */
	if (!rsrcData.empty())
	{
		out.write(reinterpret_cast<const char *>(rsrcData.data()),
				  static_cast<std::streamsize>(rsrcData.size()));
		size_t pad = padded(rsrcData.size()) - rsrcData.size();
		if (pad > 0)
		{
			std::array<uint8_t, 128> zeros{};
			out.write(reinterpret_cast<const char *>(zeros.data()),
					  static_cast<std::streamsize>(pad));
		}
	}

	out.close();

	auto typeFCC = [](uint32_t v)
	{
		char buf[5];
		buf[0] = static_cast<char>(v >> 24);
		buf[1] = static_cast<char>(v >> 16);
		buf[2] = static_cast<char>(v >> 8);
		buf[3] = static_cast<char>(v);
		buf[4] = 0;
		return std::string(buf);
	};

	std::printf("%s → %s (type='%s' creator='%s' data=%zu rsrc=%zu)\n", hostPath.c_str(),
				outPath.c_str(), typeFCC(fi.type).c_str(), typeFCC(fi.creator).c_str(),
				dataFork.size(), rsrcData.size());
	return 0;
}
