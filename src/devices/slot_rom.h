/*
	slot_rom.h — NuBus Slot ROM builder utilities

	SlotROMWriter: big-endian serializer for building slot ROM images.
	VPBlock: video parameter block for each display mode.
*/
#pragma once

#include <cstdint>
#include <cstddef>
#include <cstring>

class SlotROMWriter {
public:
	SlotROMWriter(uint8_t* buf, size_t capacity)
		: buf_(buf), capacity_(capacity), pos_(0) {}

	size_t pos() const { return pos_; }

	void seek(size_t offset) { pos_ = offset; }

	void writeByte(uint8_t v)
	{
		if (pos_ < capacity_)
			buf_[pos_] = v;
		else
			overflow_ = true;
		++pos_;
	}

	void writeWord(uint16_t v)
	{
		writeByte(v >> 8);
		writeByte(v & 0xFF);
	}

	void writeLong(uint32_t v)
	{
		writeByte(v >> 24);
		writeByte((v >> 16) & 0xFF);
		writeByte((v >> 8) & 0xFF);
		writeByte(v & 0xFF);
	}

	void writeBytes(const uint8_t* data, size_t len)
	{
		for (size_t i = 0; i < len; ++i)
			writeByte(data[i]);
	}

	/* Write a C string (null-terminated), padded to 4-byte alignment */
	void writeString(const char* s)
	{
		size_t len = std::strlen(s) + 1; /* include NUL */
		writeBytes(reinterpret_cast<const uint8_t*>(s), len);
		/* Pad to 4-byte alignment */
		while (pos_ % 4 != 0)
			writeByte(0);
	}

	/*
		Reserve 4 bytes for an sResource list entry.
		Returns the position of the reserved slot so patchOffset()
		can fill it in later.
	*/
	size_t reserve()
	{
		size_t p = pos_;
		writeLong(0);
		return p;
	}

	/*
		Fill in a reserved OSlst entry: ID byte + 24-bit self-relative
		offset from the reserved position to the current write position.
		Encoding: (id << 24) | ((current - reserved) & 0x00FFFFFF)
	*/
	void patchOffset(size_t reserved, uint8_t id)
	{
		uint32_t delta = (uint32_t)(pos_ - reserved) & 0x00FFFFFF;
		uint32_t val = ((uint32_t)id << 24) | delta;
		size_t saved = pos_;
		seek(reserved);
		writeLong(val);
		seek(saved);
	}

	/* DatLst entry: ID byte + 24-bit literal data */
	void writeDataEntry(uint8_t id, uint32_t data)
	{
		writeLong(((uint32_t)id << 24) | (data & 0x00FFFFFF));
	}

	/* End-of-list marker: 0xFF000000 */
	void writeEndOfList()
	{
		writeDataEntry(0xFF, 0x00000000);
	}

	bool overflowed() const { return overflow_ || pos_ > capacity_; }

private:
	uint8_t* buf_;
	size_t   capacity_;
	size_t   pos_;
	bool     overflow_ = false;
};

/*
	VPBlock — Video Parameters Block (46 bytes, big-endian on wire).
	Describes the framebuffer layout for a given display mode.
*/
struct VPBlock {
	uint32_t physBlockSize = 0x2E;  /* always 46 */
	uint32_t baseOffset    = 0;
	uint16_t rowBytes;
	uint16_t boundsTop     = 0;
	uint16_t boundsLeft    = 0;
	uint16_t boundsBottom;          /* height */
	uint16_t boundsRight;           /* width  */
	uint16_t version       = 0;
	uint16_t packType      = 0;
	uint32_t packSize      = 0;
	uint32_t hRes          = 0x00480000;  /* 72 dpi fixed-point */
	uint32_t vRes          = 0x00480000;
	uint16_t pixelType;     /* 0=indexed, 0x10=direct */
	uint16_t pixelSize;     /* 1,2,4,8,16,32 */
	uint16_t cmpCount;      /* 1 or 3 */
	uint16_t cmpSize;       /* 1,2,4,8 or 5 */
	uint32_t planeBytes    = 0;

	/*
		Factory: create VPBlock for a given depth (0..5) and resolution.
		depth 0 = 1 bpp, 1 = 2 bpp, ... 5 = 32 bpp.
	*/
	static VPBlock forMode(int depth, uint16_t width, uint16_t height)
	{
		VPBlock vp;
		int bpp = 1 << depth;

		vp.boundsBottom = height;
		vp.boundsRight  = width;
		vp.rowBytes     = (uint16_t)((uint32_t)width * bpp / 8);
		vp.pixelSize    = bpp;

		if (depth < 4) {
			/* Indexed (CLUT) modes: 1,2,4,8 bpp */
			vp.pixelType = 0;
			vp.cmpCount  = 1;
			vp.cmpSize   = bpp;
		} else {
			/* Direct modes: 16 bpp (5-5-5) or 32 bpp (xRGB) */
			vp.pixelType = 0x10;
			vp.cmpCount  = 3;
			vp.cmpSize   = (depth == 4) ? 5 : 8;
		}

		return vp;
	}

	/* Serialize the 46-byte block to a SlotROMWriter (big-endian) */
	void writeTo(SlotROMWriter& w) const
	{
		w.writeLong(physBlockSize);
		w.writeLong(baseOffset);
		w.writeWord(rowBytes);
		w.writeWord(boundsTop);
		w.writeWord(boundsLeft);
		w.writeWord(boundsBottom);
		w.writeWord(boundsRight);
		w.writeWord(version);
		w.writeWord(packType);
		w.writeLong(packSize);
		w.writeLong(hRes);
		w.writeLong(vRes);
		w.writeWord(pixelType);
		w.writeWord(pixelSize);
		w.writeWord(cmpCount);
		w.writeWord(cmpSize);
		w.writeLong(planeBytes);
	}

	/* Write VPBlock directly to guest memory at the given address */
	static void writeToGuest(int depth, uint16_t width, uint16_t height,
	                          uint32_t guestPtr);
};
