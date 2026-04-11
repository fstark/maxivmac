/*
	test_slip — Unit tests for the RFC 1055 SLIP codec.
*/

#include "devices/slip.h"
#include <cassert>
#include <cstdio>

/* Helper: feed all bytes from a framed buffer into a decoder.
   Returns the number of complete packets signalled. */
static int feedAll(slip::Decoder &dec, const std::vector<uint8_t> &framed)
{
	int n = 0;
	for (uint8_t b : framed)
		n += dec.feed(b) ? 1 : 0;
	return n;
}

static void test_roundtrip()
{
	uint8_t pkt[] = {0x45, 0x00, 0x00, 0x1C, 0xC0, 0xDB, 0x01, 0x02};
	std::vector<uint8_t> framed;
	slip::encode(pkt, sizeof(pkt), framed);

	slip::Decoder dec;
	assert(feedAll(dec, framed) == 1);
	auto result = dec.packet();
	assert(result.size() == sizeof(pkt));
	for (size_t i = 0; i < sizeof(pkt); ++i)
		assert(result[i] == pkt[i]);
}

static void test_empty_frames()
{
	slip::Decoder dec;
	/* Multiple ENDs in a row -> no packet */
	assert(!dec.feed(slip::END));
	assert(!dec.feed(slip::END));
	assert(!dec.feed(slip::END));
}

static void test_all_special_bytes()
{
	/* Packet entirely made of END and ESC bytes */
	uint8_t pkt[] = {slip::END, slip::ESC, slip::END, slip::ESC};
	std::vector<uint8_t> framed;
	slip::encode(pkt, sizeof(pkt), framed);

	slip::Decoder dec;
	assert(feedAll(dec, framed) == 1);
	auto result = dec.packet();
	assert(result.size() == sizeof(pkt));
	for (size_t i = 0; i < sizeof(pkt); ++i)
		assert(result[i] == pkt[i]);
}

static void test_multiple_packets()
{
	/* Two packets back-to-back in one stream */
	uint8_t pkt1[] = {0x01, 0x02, 0x03};
	uint8_t pkt2[] = {0x04, 0x05};
	std::vector<uint8_t> framed;
	slip::encode(pkt1, sizeof(pkt1), framed);
	slip::encode(pkt2, sizeof(pkt2), framed);

	slip::Decoder dec;
	int count = 0;
	for (uint8_t b : framed)
	{
		if (dec.feed(b))
		{
			auto result = dec.packet();
			if (count == 0)
			{
				assert(result.size() == sizeof(pkt1));
				for (size_t i = 0; i < sizeof(pkt1); ++i)
					assert(result[i] == pkt1[i]);
			}
			else
			{
				assert(result.size() == sizeof(pkt2));
				for (size_t i = 0; i < sizeof(pkt2); ++i)
					assert(result[i] == pkt2[i]);
			}
			count++;
		}
	}
	assert(count == 2);
}

static void test_zero_length_packet()
{
	/* Encoding zero bytes should produce END END with no payload. */
	std::vector<uint8_t> framed;
	slip::encode(nullptr, 0, framed);
	assert(framed.size() == 2);
	assert(framed[0] == slip::END);
	assert(framed[1] == slip::END);

	/* Decoding that should yield no packet (empty). */
	slip::Decoder dec;
	assert(feedAll(dec, framed) == 0);
}

int main()
{
	test_roundtrip();
	test_empty_frames();
	test_all_special_bytes();
	test_multiple_packets();
	test_zero_length_packet();
	std::printf("SLIP: all tests passed\n");
	return 0;
}
