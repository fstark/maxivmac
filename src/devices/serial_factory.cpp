/*
	serial_factory — Create serial backends from command-line mode strings.
*/

#include "devices/serial_factory.h"
#include "devices/serial_loopback.h"
#include "devices/serial_file.h"
#ifndef _WIN32
#include "devices/serial_pty.h"
#endif
#if HAVE_SLIRP
#include "devices/serial_slirp.h"
#endif
#include <cstdio>
#include <cstring>

/* Parse "key=value" pairs from a comma-separated string after the prefix.
   E.g. "tx=/tmp/out.bin,rx=/tmp/in.bin" → tx="/tmp/out.bin", rx="/tmp/in.bin" */
static void parseKV(const std::string &s, std::string &tx, std::string &rx)
{
	size_t pos = 0;
	while (pos < s.size())
	{
		size_t eq = s.find('=', pos);
		if (eq == std::string::npos) break;
		size_t comma = s.find(',', eq);
		std::string key = s.substr(pos, eq - pos);
		std::string val =
			s.substr(eq + 1, (comma == std::string::npos) ? std::string::npos : comma - eq - 1);
		if (key == "tx")
			tx = val;
		else if (key == "rx")
			rx = val;
		pos = (comma == std::string::npos) ? s.size() : comma + 1;
	}
}

std::unique_ptr<SerialBackend> CreateSerialBackend(const std::string &mode, int chan)
{
	if (mode.empty()) return nullptr;

	if (mode == "loopback")
	{
		fprintf(stderr, "[SER] ch%d: loopback backend attached\n", chan);
		return std::make_unique<LoopbackBackend>();
	}

	if (mode.compare(0, 5, "file:") == 0)
	{
		std::string tx, rx;
		parseKV(mode.substr(5), tx, rx);
		if (tx.empty() && rx.empty())
		{
			fprintf(stderr, "[SER] ch%d: file backend needs tx= and/or rx= paths\n", chan);
			return nullptr;
		}
		fprintf(stderr, "[SER] ch%d: file backend", chan);
		if (!tx.empty()) fprintf(stderr, " tx=%s", tx.c_str());
		if (!rx.empty()) fprintf(stderr, " rx=%s", rx.c_str());
		fprintf(stderr, "\n");
		return std::make_unique<FileBackend>(tx, rx);
	}

#ifndef _WIN32
	if (mode == "pty")
	{
		return std::make_unique<PtyBackend>(chan);
	}
#endif

#if HAVE_SLIRP
	if (mode == "slip")
	{
		fprintf(stderr, "[SER] ch%d: SLIP/libslirp backend\n", chan);
		return std::make_unique<SlirpBackend>();
	}
#endif

	fprintf(stderr, "[SER] ch%d: unknown serial mode '%s'\n", chan, mode.c_str());
	return nullptr;
}
