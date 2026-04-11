/*
	PtyBackend — host pseudo-terminal connected to an SCC channel.

	Creates a PTY pair; the slave path is printed to stderr so the user
	can attach a terminal emulator (screen, minicom, etc.).

	POSIX only (macOS, Linux).
*/
#pragma once

#include "devices/serial_backend.h"

#ifndef _WIN32

#include <cstdio>
#include <cstdlib>
#include <fcntl.h>
#include <poll.h>
#include <unistd.h>
#include <queue>

class PtyBackend : public SerialBackend {
public:
	explicit PtyBackend(int chan)
	{
		masterFd_ = posix_openpt(O_RDWR | O_NOCTTY);
		if (masterFd_ < 0) { perror("[SER] posix_openpt"); return; }
		if (grantpt(masterFd_) < 0)  { perror("[SER] grantpt");  close(masterFd_); masterFd_ = -1; return; }
		if (unlockpt(masterFd_) < 0) { perror("[SER] unlockpt"); close(masterFd_); masterFd_ = -1; return; }

		const char* slavePath = ptsname(masterFd_);
		if (!slavePath) { perror("[SER] ptsname"); close(masterFd_); masterFd_ = -1; return; }

		slavePath_ = slavePath;
		fprintf(stderr, "[SER] ch%d: PTY backend -> %s\n", chan, slavePath_.c_str());

		/* Set master to non-blocking. */
		int flags = fcntl(masterFd_, F_GETFL);
		if (flags >= 0) fcntl(masterFd_, F_SETFL, flags | O_NONBLOCK);
	}

	~PtyBackend() override
	{
		if (masterFd_ >= 0) close(masterFd_);
	}

	void txByte(uint8_t byte) override
	{
		if (masterFd_ >= 0)
			(void)write(masterFd_, &byte, 1);
	}

	bool rxReady() override { return !rxQueue_.empty(); }

	uint8_t rxByte() override
	{
		uint8_t b = rxQueue_.front();
		rxQueue_.pop();
		return b;
	}

	void poll() override
	{
		if (masterFd_ < 0) return;

		/* Non-blocking read: drain all available bytes into queue. */
		uint8_t buf[256];
		for (;;) {
			ssize_t n = read(masterFd_, buf, sizeof(buf));
			if (n <= 0) break;
			for (ssize_t i = 0; i < n; ++i)
				rxQueue_.push(buf[i]);
		}
	}

	const char* name() const override { return "pty"; }
	const std::string& slavePath() const { return slavePath_; }

private:
	int masterFd_ = -1;
	std::string slavePath_;
	std::queue<uint8_t> rxQueue_;
};

#endif /* !_WIN32 */
