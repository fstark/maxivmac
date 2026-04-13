/*
	test_slip — Unit tests for the RFC 1055 SLIP codec.
*/

#include <doctest/doctest.h>
#include "devices/slip.h"

/* Helper: feed all bytes from a framed buffer into a decoder.
   Returns the number of complete packets signalled. */
static int feedAll(slip::Decoder &dec, const std::vector<uint8_t> &framed)
{
	int n = 0;
	for (uint8_t b : framed)
		n += dec.feed(b) ? 1 : 0;
	return n;
}

TEST_CASE("SLIP roundtrip")
{
	uint8_t pkt[] = {0x45, 0x00, 0x00, 0x1C, 0xC0, 0xDB, 0x01, 0x02};
	std::vector<uint8_t> framed;
	slip::encode(pkt, sizeof(pkt), framed);

	slip::Decoder dec;
	CHECK(feedAll(dec, framed) == 1);
	auto result = dec.packet();
	REQUIRE(result.size() == sizeof(pkt));
	for (size_t i = 0; i < sizeof(pkt); ++i)
		CHECK(result[i] == pkt[i]);
}

TEST_CASE("SLIP empty frames")
{
	slip::Decoder dec;
	/* Multiple ENDs in a row -> no packet */
	CHECK_FALSE(dec.feed(slip::END));
	CHECK_FALSE(dec.feed(slip::END));
	CHECK_FALSE(dec.feed(slip::END));
}

TEST_CASE("SLIP all special bytes")
{
	/* Packet entirely made of END and ESC bytes */
	uint8_t pkt[] = {slip::END, slip::ESC, slip::END, slip::ESC};
	std::vector<uint8_t> framed;
	slip::encode(pkt, sizeof(pkt), framed);

	slip::Decoder dec;
	CHECK(feedAll(dec, framed) == 1);
	auto result = dec.packet();
	REQUIRE(result.size() == sizeof(pkt));
	for (size_t i = 0; i < sizeof(pkt); ++i)
		CHECK(result[i] == pkt[i]);
}

TEST_CASE("SLIP multiple packets")
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
				REQUIRE(result.size() == sizeof(pkt1));
				for (size_t i = 0; i < sizeof(pkt1); ++i)
					CHECK(result[i] == pkt1[i]);
			}
			else
			{
				REQUIRE(result.size() == sizeof(pkt2));
				for (size_t i = 0; i < sizeof(pkt2); ++i)
					CHECK(result[i] == pkt2[i]);
			}
			count++;
		}
	}
	CHECK(count == 2);
}

TEST_CASE("SLIP zero length packet")
{
	/* Encoding zero bytes should produce END END with no payload. */
	std::vector<uint8_t> framed;
	slip::encode(nullptr, 0, framed);
	REQUIRE(framed.size() == 2);
	CHECK(framed[0] == slip::END);
	CHECK(framed[1] == slip::END);

	/* Decoding that should yield no packet (empty). */
	slip::Decoder dec;
	CHECK(feedAll(dec, framed) == 0);
}
