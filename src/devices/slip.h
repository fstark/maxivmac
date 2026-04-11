/*
	slip.h — RFC 1055 SLIP framing codec.

	Pure library, no emulator dependencies.  Suitable for unit testing.
*/
#pragma once

#include <cstdint>
#include <cstdio>
#include <vector>

/* -----------------------------------------------------------------------
   LOG — SLP subsystem (SLIP framing)
   ----------------------------------------------------------------------- */
#define SLP_dolog 1

#if SLP_dolog
#define SLP_LOG(fmt, ...) std::fprintf(stderr, "[SLP] " fmt "\n", ##__VA_ARGS__)
#else
#define SLP_LOG(fmt, ...) ((void)0)
#endif

namespace slip
{

/* SLIP special bytes (RFC 1055) */
constexpr uint8_t END = 0xC0;
constexpr uint8_t ESC = 0xDB;
constexpr uint8_t ESC_END = 0xDC;
constexpr uint8_t ESC_ESC = 0xDD;

/* Encode an IP packet into SLIP-framed bytes appended to `out`. */
inline void encode(const uint8_t *pkt, size_t len, std::vector<uint8_t> &out)
{
	size_t before = out.size();
	out.push_back(END); /* flush any line noise */
	for (size_t i = 0; i < len; ++i)
	{
		switch (pkt[i])
		{
			case END:
				out.push_back(ESC);
				out.push_back(ESC_END);
				break;
			case ESC:
				out.push_back(ESC);
				out.push_back(ESC_ESC);
				break;
			default:
				out.push_back(pkt[i]);
				break;
		}
	}
	out.push_back(END);
	SLP_LOG("encode: %zu bytes -> %zu framed", len, out.size() - before);
}

/*
   Stateful decoder: feed bytes one at a time.  When a complete IP packet
   has been assembled, feed() returns true and packet() retrieves it.
*/
class Decoder
{
public:
	/* Feed one byte.  Returns true if a complete packet is now available. */
	bool feed(uint8_t byte)
	{
		if (byte == END)
		{
			if (!accum_.empty())
			{
				SLP_LOG("decode: complete packet %zu bytes", accum_.size());
				return true; /* packet complete */
			}
			return false; /* empty frame (inter-packet END) — ignore */
		}
		if (byte == ESC)
		{
			escaped_ = true;
			return false;
		}
		if (escaped_)
		{
			escaped_ = false;
			switch (byte)
			{
				case ESC_END:
					accum_.push_back(END);
					break;
				case ESC_ESC:
					accum_.push_back(ESC);
					break;
				default:
					accum_.push_back(byte);
					break; /* protocol error — be lenient */
			}
			return false;
		}
		accum_.push_back(byte);
		return false;
	}

	/* Retrieve the completed packet and clear the buffer. */
	std::vector<uint8_t> packet()
	{
		std::vector<uint8_t> pkt;
		pkt.swap(accum_);
		return pkt;
	}

	void reset()
	{
		accum_.clear();
		escaped_ = false;
	}

private:
	std::vector<uint8_t> accum_;
	bool escaped_ = false;
};

} /* namespace slip */
