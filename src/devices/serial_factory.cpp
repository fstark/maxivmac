/*
	serial_factory — Create serial backends from command-line mode strings.
*/

#include "devices/serial_factory.h"
#include "devices/serial_loopback.h"
#include <cstdio>
#include <cstring>

std::unique_ptr<SerialBackend> CreateSerialBackend(const std::string& mode, int chan)
{
	if (mode.empty()) return nullptr;

	if (mode == "loopback") {
		fprintf(stderr, "[SER] ch%d: loopback backend attached\n", chan);
		return std::make_unique<LoopbackBackend>();
	}

	/* Future backends: file:tx=..., pty, device:... */

	fprintf(stderr, "[SER] ch%d: unknown serial mode '%s'\n", chan, mode.c_str());
	return nullptr;
}
