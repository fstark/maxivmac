/*
	dbg_io.cpp — StdioIO and SocketIO implementations
*/

#include "debugger/dbg_io.h"

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <vector>

/* ── StdioIO ────────────────────────────────────────── */

class StdioIO final : public DbgIO
{
public:
	bool readLine(char *buf, size_t len) override
	{
		return std::fgets(buf, static_cast<int>(len), stdin) != nullptr;
	}

	void write(const char *fmt, ...) override
	{
		std::va_list ap;
		va_start(ap, fmt);
		std::vprintf(fmt, ap);
		va_end(ap);
	}

	void endResponse() override {}

	void flush() override { std::fflush(stdout); }
};

std::unique_ptr<DbgIO> CreateStdioIO()
{
	return std::make_unique<StdioIO>();
}

/* ── SocketIO ───────────────────────────────────────── */

class SocketIO final : public DbgIO
{
public:
	explicit SocketIO(int listenFd) : listenFd_(listenFd) {}

	~SocketIO() override
	{
		if (clientFd_ >= 0) close(clientFd_);
		if (listenFd_ >= 0) close(listenFd_);
	}

	bool readLine(char *buf, size_t len) override
	{
		if (clientFd_ < 0) return false;

		size_t outPos = 0;
		while (outPos < len - 1)
		{
			/* Drain from recv buffer first */
			while (recvPos_ < recvLen_ && outPos < len - 1)
			{
				char c = recvBuf_[recvPos_++];
				buf[outPos++] = c;
				if (c == '\n')
				{
					buf[outPos] = '\0';
					return true;
				}
			}
			/* Need more data */
			ssize_t n = recv(clientFd_, recvBuf_, sizeof(recvBuf_), 0);
			if (n <= 0) return false; /* client disconnected or error */
			recvPos_ = 0;
			recvLen_ = static_cast<size_t>(n);
		}
		buf[outPos] = '\0';
		return outPos > 0;
	}

	void write(const char *fmt, ...) override
	{
		if (clientFd_ < 0) return;

		char stackBuf[4096];
		std::va_list ap;
		va_start(ap, fmt);
		int n = std::vsnprintf(stackBuf, sizeof(stackBuf), fmt, ap);
		va_end(ap);

		if (n < 0) return;
		if (static_cast<size_t>(n) < sizeof(stackBuf))
		{
			send(clientFd_, stackBuf, static_cast<size_t>(n), 0);
		}
		else
		{
			/* Allocate for large output */
			std::vector<char> heapBuf(static_cast<size_t>(n) + 1);
			va_start(ap, fmt);
			std::vsnprintf(heapBuf.data(), heapBuf.size(), fmt, ap);
			va_end(ap);
			send(clientFd_, heapBuf.data(), static_cast<size_t>(n), 0);
		}
	}

	void endResponse() override
	{
		if (clientFd_ < 0) return;
		send(clientFd_, "\x04\n", 2, 0);
	}

	void flush() override {} /* send is unbuffered */

	bool isSocket() const override { return true; }

	bool acceptClient() override
	{
		if (listenFd_ < 0) return false;
		clientFd_ = accept(listenFd_, nullptr, nullptr);
		if (clientFd_ < 0)
		{
			std::perror("accept");
			return false;
		}
		recvPos_ = 0;
		recvLen_ = 0;
		return true;
	}

	void closeClient() override
	{
		if (clientFd_ >= 0)
		{
			close(clientFd_);
			clientFd_ = -1;
		}
		recvPos_ = 0;
		recvLen_ = 0;
	}

private:
	int listenFd_ = -1;
	int clientFd_ = -1;
	char recvBuf_[4096]{};
	size_t recvPos_ = 0;
	size_t recvLen_ = 0;
};

std::unique_ptr<DbgIO> CreateSocketIO(int listenFd)
{
	return std::make_unique<SocketIO>(listenFd);
}

/* ── Listen socket creation ─────────────────────────── */

static std::string s_socketCleanupPath;

static void CleanupSocket()
{
	if (!s_socketCleanupPath.empty()) unlink(s_socketCleanupPath.c_str());
}

int CreateListenSocket(const std::string &path)
{
	int fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (fd < 0)
	{
		std::perror("socket");
		return -1;
	}

	/* Remove stale socket file (ignore errors) */
	unlink(path.c_str());

	struct sockaddr_un addr{};
	addr.sun_family = AF_UNIX;
	if (path.size() >= sizeof(addr.sun_path))
	{
		std::fprintf(stderr, "Socket path too long: %s\n", path.c_str());
		close(fd);
		return -1;
	}
	std::strncpy(addr.sun_path, path.c_str(), sizeof(addr.sun_path) - 1);

	if (bind(fd, reinterpret_cast<struct sockaddr *>(&addr), sizeof(addr)) < 0)
	{
		std::perror("bind");
		close(fd);
		return -1;
	}

	if (listen(fd, 1) < 0)
	{
		std::perror("listen");
		close(fd);
		return -1;
	}

	/* Register cleanup on exit */
	s_socketCleanupPath = path;
	std::atexit(CleanupSocket);

	return fd;
}
