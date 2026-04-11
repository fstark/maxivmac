/*
	SerialBackend — abstract interface for SCC serial port backends.

	Each SCC channel (A = modem, B = printer) can optionally hold a
	pointer to a SerialBackend.  When attached, TX bytes from the guest
	are forwarded to the backend, and the backend provides RX bytes
	back to the SCC.
*/
#pragma once

#include <cstdint>

class SerialBackend {
public:
	virtual ~SerialBackend() = default;

	/* Called when the guest writes a byte to WR8 (transmit buffer). */
	virtual void txByte(uint8_t byte) = 0;

	/* Returns true if at least one byte is available for the guest. */
	virtual bool rxReady() = 0;

	/* Dequeue and return the next received byte.  Only call when rxReady(). */
	virtual uint8_t rxByte() = 0;

	/* Called once per 1/60s tick.  The backend should do any periodic
	   housekeeping here (poll file descriptors, drain host buffers). */
	virtual void poll() = 0;

	/* Human-readable name for logging. */
	virtual const char* name() const = 0;
};
