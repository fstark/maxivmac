/*
	serial_slirp.h — SLIP networking backend using libslirp.

	When HAVE_SLIRP is defined, provides a SerialBackend that bridges
	SLIP-framed serial I/O to libslirp's user-space TCP/IP stack.
*/
#pragma once

#if HAVE_SLIRP

#include "devices/serial_backend.h"
#include "devices/slip.h"
#include <libslirp.h>
#include <memory>
#include <queue>
#include <vector>

class SlirpBackend : public SerialBackend
{
public:
	SlirpBackend();
	~SlirpBackend() override;

	void txByte(uint8_t byte) override;
	bool rxReady() override;
	uint8_t rxByte() override;
	void poll() override;
	const char *name() const override { return "slip"; }

	/* libslirp callback: a packet arrived from the network */
	void onSlirpOutput(const uint8_t *pkt, size_t len);

private:
	Slirp *slirp_ = nullptr;
	slip::Decoder decoder_;			/* guest TX: decode SLIP → IP */
	std::queue<uint8_t> txToGuest_; /* guest RX: SLIP-encoded queue */
};

#endif /* HAVE_SLIRP */
