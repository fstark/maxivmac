#include "icon_builder.h"
#include "platform/common/path_utils.h"
#include "stb_image.h"
#include <array>
#include <cstring>

/* Apple 8-bit system palette, same as tools/icon_extract/mac_palette.h.
   Duplicated here to keep storage/ self-contained from tools/. */
#include "mac_system_palette.h"

namespace storage
{
namespace
{

/* ── Pixel helpers ────────────────────────────────── */

struct RGBA
{
	uint8_t r, g, b, a;
};

RGBA pixel(const uint8_t *rgba, int x, int y, int stride)
{
	const uint8_t *p = rgba + (y * stride + x) * 4;
	return {p[0], p[1], p[2], p[3]};
}

bool isOpaque(RGBA c)
{
	return c.a >= 128;
}

bool isBlack(RGBA c)
{
	int lum = (c.r * 77 + c.g * 150 + c.b * 29) >> 8;
	return lum < 128;
}

/* Nearest-match into the Mac system palette (brute-force, 32×32 is tiny) */
uint8_t nearestPaletteIndex(RGBA c)
{
	int bestDist = INT32_MAX;
	uint8_t bestIdx = 0xFF; /* black */
	for (int i = 0; i < 256; ++i)
	{
		uint32_t p = kMacSystemPalette[i];
		int dr = c.r - static_cast<int>((p >> 24) & 0xFF);
		int dg = c.g - static_cast<int>((p >> 16) & 0xFF);
		int db = c.b - static_cast<int>((p >> 8) & 0xFF);
		int dist = dr * dr + dg * dg + db * db;
		if (dist < bestDist)
		{
			bestDist = dist;
			bestIdx = static_cast<uint8_t>(i);
		}
	}
	return bestIdx;
}

/* Box-filter downsample 32×32 → 16×16 (average 2×2 blocks) */
std::array<uint8_t, 16 * 16 * 4> downsample16(const uint8_t *rgba32)
{
	std::array<uint8_t, 16 * 16 * 4> out{};
	for (int y = 0; y < 16; ++y)
	{
		for (int x = 0; x < 16; ++x)
		{
			int r = 0, g = 0, b = 0, a = 0;
			for (int dy = 0; dy < 2; ++dy)
			{
				for (int dx = 0; dx < 2; ++dx)
				{
					const uint8_t *p = rgba32 + ((y * 2 + dy) * 32 + (x * 2 + dx)) * 4;
					r += p[0];
					g += p[1];
					b += p[2];
					a += p[3];
				}
			}
			uint8_t *o = out.data() + (y * 16 + x) * 4;
			o[0] = static_cast<uint8_t>(r / 4);
			o[1] = static_cast<uint8_t>(g / 4);
			o[2] = static_cast<uint8_t>(b / 4);
			o[3] = static_cast<uint8_t>(a / 4);
		}
	}
	return out;
}

/* ── ICN# / ics# builders ────────────────────────── */

/* Build 1-bit icon data + mask from RGBA pixels.
   Icon: black where luminance < 128 AND opaque.
   Mask: 1 where opaque (alpha >= 128). */
std::vector<uint8_t> buildBitmapAndMask(const uint8_t *rgba, int size)
{
	int rowBytes = size / 8;
	int total = rowBytes * size * 2; /* icon + mask */
	std::vector<uint8_t> out(total, 0);

	uint8_t *icon = out.data();
	uint8_t *mask = out.data() + rowBytes * size;

	for (int y = 0; y < size; ++y)
	{
		for (int x = 0; x < size; ++x)
		{
			RGBA c = pixel(rgba, x, y, size);
			int byteIdx = y * rowBytes + x / 8;
			int bit = 7 - (x % 8);

			if (isOpaque(c) && isBlack(c)) icon[byteIdx] |= (1 << bit);
			if (isOpaque(c)) mask[byteIdx] |= (1 << bit);
		}
	}
	return out;
}

/* ── icl8 / ics8 builders ─────────────────────────── */

std::vector<uint8_t> buildPalette8(const uint8_t *rgba, int size)
{
	std::vector<uint8_t> out(size * size);
	for (int y = 0; y < size; ++y)
	{
		for (int x = 0; x < size; ++x)
		{
			RGBA c = pixel(rgba, x, y, size);
			if (!isOpaque(c))
				out[y * size + x] = 0xFF; /* white / transparent */
			else
				out[y * size + x] = nearestPaletteIndex(c);
		}
	}
	return out;
}

/* ── Resource fork builder ────────────────────────── */

/* Big-endian writers */
void writeBE16(std::vector<uint8_t> &v, uint16_t val)
{
	v.push_back(static_cast<uint8_t>(val >> 8));
	v.push_back(static_cast<uint8_t>(val));
}

void writeBE32(std::vector<uint8_t> &v, uint32_t val)
{
	v.push_back(static_cast<uint8_t>(val >> 24));
	v.push_back(static_cast<uint8_t>(val >> 16));
	v.push_back(static_cast<uint8_t>(val >> 8));
	v.push_back(static_cast<uint8_t>(val));
}

void patchBE16(std::vector<uint8_t> &v, size_t off, uint16_t val)
{
	v[off] = static_cast<uint8_t>(val >> 8);
	v[off + 1] = static_cast<uint8_t>(val);
}

void patchBE32(std::vector<uint8_t> &v, size_t off, uint32_t val)
{
	v[off] = static_cast<uint8_t>(val >> 24);
	v[off + 1] = static_cast<uint8_t>(val >> 16);
	v[off + 2] = static_cast<uint8_t>(val >> 8);
	v[off + 3] = static_cast<uint8_t>(val);
}

constexpr int16_t kCustomIconID = -16455; /* $BFB9 */

struct ResEntry
{
	uint32_t type; /* 'ICN#', 'icl8', etc. */
	std::vector<uint8_t> data;
};

/*
	Build a complete resource fork from a list of resources, all sharing
	the same resource ID.  Layout:

	  [Header: 16 bytes]
		dataOffset, mapOffset, dataLen, mapLen

	  [Data section]
		For each resource: 4-byte length prefix + data bytes

	  [Map section]
		Copy of header (16 bytes)
		nextMap(4) + fileRef(2) + attrs(2) = 8 bytes
		typeListOffset(2) + nameListOffset(2) = 4 bytes
		Type list:
		  numTypes-1 (2)
		  For each type: type(4) + numRes-1(2) + refListOffset(2)
		Ref list:
		  For each resource: id(2) + nameOff(2) + attrs+dataOff(4) + reserved(4)
		Name list: empty
*/
std::vector<uint8_t> buildResourceFork(const std::vector<ResEntry> &entries, int16_t resID)
{
	std::vector<uint8_t> fork;

	/* === Header placeholder (16 bytes) === */
	size_t headerOff = fork.size();
	fork.resize(headerOff + 16, 0);

	/* Reserve 112 bytes after header (resource fork convention) */
	fork.resize(fork.size() + 112, 0);

	/* Reserved 128 bytes after the 112 (system use) */
	fork.resize(fork.size() + 128, 0);

	/* === Data section === */
	size_t dataStart = fork.size();
	std::vector<uint32_t> dataOffsets; /* offset from dataStart for each entry */

	for (auto &e : entries)
	{
		dataOffsets.push_back(static_cast<uint32_t>(fork.size() - dataStart));
		writeBE32(fork, static_cast<uint32_t>(e.data.size()));
		fork.insert(fork.end(), e.data.begin(), e.data.end());
	}

	uint32_t dataLen = static_cast<uint32_t>(fork.size() - dataStart);

	/* === Map section === */
	size_t mapStart = fork.size();

	/* Copy of header (16 bytes, filled later) */
	fork.resize(fork.size() + 16, 0);

	/* nextMap(4) + fileRef(2) + attrs(2) */
	writeBE32(fork, 0);
	writeBE16(fork, 0);
	writeBE16(fork, 0);

	/* typeListOffset and nameListOffset (relative to map start) */
	size_t typeListOffPos = fork.size();
	writeBE16(fork, 0); /* patched below */
	size_t nameListOffPos = fork.size();
	writeBE16(fork, 0); /* patched below */

	/* === Type list === */
	size_t typeListStart = fork.size();
	uint16_t typeListOff = static_cast<uint16_t>(typeListStart - mapStart);

	/* Deduplicate types (each type appears once in the type list) */
	struct TypeInfo
	{
		uint32_t type;
		std::vector<size_t> entryIndices; /* indices into entries[] */
	};
	std::vector<TypeInfo> types;
	for (size_t i = 0; i < entries.size(); ++i)
	{
		bool found = false;
		for (auto &t : types)
		{
			if (t.type == entries[i].type)
			{
				t.entryIndices.push_back(i);
				found = true;
				break;
			}
		}
		if (!found) types.push_back({entries[i].type, {i}});
	}

	/* numTypes - 1 */
	writeBE16(fork, static_cast<uint16_t>(types.size() - 1));

	/* Type entries: type(4) + count-1(2) + refListOffset(2) */
	size_t typeEntriesStart = fork.size();
	for (size_t t = 0; t < types.size(); ++t)
	{
		writeBE32(fork, types[t].type);
		writeBE16(fork, static_cast<uint16_t>(types[t].entryIndices.size() - 1));
		writeBE16(fork, 0); /* patched below */
	}

	/* === Reference lists === */
	for (size_t t = 0; t < types.size(); ++t)
	{
		uint16_t refOff = static_cast<uint16_t>(fork.size() - typeListStart);
		patchBE16(fork, typeEntriesStart + t * 8 + 6, refOff);

		for (size_t idx : types[t].entryIndices)
		{
			writeBE16(fork, static_cast<uint16_t>(resID)); /* id */
			writeBE16(fork, 0xFFFF);					   /* nameOffset = -1 (none) */
			/* attrs (1 byte) + data offset (3 bytes) */
			uint32_t doff = dataOffsets[idx];
			writeBE32(fork, doff & 0x00FFFFFF); /* attrs=0, 24-bit offset */
			writeBE32(fork, 0);					/* reserved */
		}
	}

	/* === Name list (empty) === */
	size_t nameListStart = fork.size();
	uint16_t nameListOff = static_cast<uint16_t>(nameListStart - mapStart);

	uint32_t mapLen = static_cast<uint32_t>(fork.size() - mapStart);

	/* === Patch offsets === */
	uint32_t dataOffset = static_cast<uint32_t>(dataStart);
	uint32_t mapOffset = static_cast<uint32_t>(mapStart);

	/* Header */
	patchBE32(fork, headerOff + 0, dataOffset);
	patchBE32(fork, headerOff + 4, mapOffset);
	patchBE32(fork, headerOff + 8, dataLen);
	patchBE32(fork, headerOff + 12, mapLen);

	/* Map header copy */
	patchBE32(fork, mapStart + 0, dataOffset);
	patchBE32(fork, mapStart + 4, mapOffset);
	patchBE32(fork, mapStart + 8, dataLen);
	patchBE32(fork, mapStart + 12, mapLen);

	/* Type list offset and name list offset (relative to map start) */
	patchBE16(fork, typeListOffPos, typeListOff);
	patchBE16(fork, nameListOffPos, nameListOff);

	return fork;
}

} // anonymous namespace

/* ── Public API ───────────────────────────────────── */

std::vector<uint8_t> BuildIconResourceFork(const std::filesystem::path &bwPath,
										   const std::filesystem::path &colorPath)
{
	int bwW = 0, bwH = 0, bwComp = 0;
	uint8_t *bwPixels = stbi_load(bwPath.string().c_str(), &bwW, &bwH, &bwComp, 4);
	if (!bwPixels || bwW != 32 || bwH != 32)
	{
		printf("[VIcon] failed to load BW: %s (%dx%d, ptr=%p)\n", path_str(bwPath).c_str(), bwW,
			   bwH,
			   (void *)bwPixels);
		if (bwPixels) stbi_image_free(bwPixels);
		return {};
	}

	int colW = 0, colH = 0, colComp = 0;
	uint8_t *colPixels = stbi_load(colorPath.string().c_str(), &colW, &colH, &colComp, 4);
	if (!colPixels || colW != 32 || colH != 32)
	{
		printf("[VIcon] failed to load color: %s (%dx%d, ptr=%p)\n",
			   path_str(colorPath).c_str(), colW, colH,
			   (void *)colPixels);
		stbi_image_free(bwPixels);
		if (colPixels) stbi_image_free(colPixels);
		return {};
	}

	/* 32×32 resources */
	auto icnHash = buildBitmapAndMask(bwPixels, 32); /* ICN# : 256 bytes */
	auto icl8 = buildPalette8(colPixels, 32);		 /* icl8 : 1024 bytes */

	/* 16×16 versions (downsampled) */
	auto bw16 = downsample16(bwPixels);
	auto col16 = downsample16(colPixels);
	auto icsHash = buildBitmapAndMask(bw16.data(), 16); /* ics# : 64 bytes */
	auto ics8 = buildPalette8(col16.data(), 16);		/* ics8 : 256 bytes */

	stbi_image_free(bwPixels);
	stbi_image_free(colPixels);

	std::vector<ResEntry> entries;
	entries.push_back({0x49434E23, std::move(icnHash)}); /* 'ICN#' */
	entries.push_back({0x69636C38, std::move(icl8)});	 /* 'icl8' */
	entries.push_back({0x69637323, std::move(icsHash)}); /* 'ics#' */
	entries.push_back({0x69637338, std::move(ics8)});	 /* 'ics8' */

	return buildResourceFork(entries, kCustomIconID);
}

} // namespace storage
