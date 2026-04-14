/*
	dbg_client.cpp — Debug client entry point (maxivmac debug)

	Thin client that connects to a running --debugserver via Unix
	domain socket.  Supports one-shot commands and interactive mode.
*/

#include "debugger/dbg_io.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <glob.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

/* ── Socket auto-discovery ──────────────────────────── */

static std::string FindSocket()
{
	glob_t g;
	if (glob("/tmp/maxivmac-dbg-*.sock", 0, nullptr, &g) != 0)
	{
		std::fprintf(stderr, "No debug server found.\n");
		return {};
	}
	if (g.gl_pathc == 1)
	{
		std::string result = g.gl_pathv[0];
		globfree(&g);
		return result;
	}
	std::fprintf(stderr, "Multiple debug servers found:\n");
	for (size_t i = 0; i < g.gl_pathc; ++i)
		std::fprintf(stderr, "  %s\n", g.gl_pathv[i]);
	std::fprintf(stderr, "Use --socket=PATH to select one.\n");
	globfree(&g);
	return {};
}

/* ── Connect to socket ──────────────────────────────── */

static int ConnectToSocket(const std::string &path)
{
	int fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (fd < 0)
	{
		std::perror("socket");
		return -1;
	}

	struct sockaddr_un addr{};
	addr.sun_family = AF_UNIX;
	std::strncpy(addr.sun_path, path.c_str(), sizeof(addr.sun_path) - 1);

	if (connect(fd, reinterpret_cast<struct sockaddr *>(&addr), sizeof(addr)) < 0)
	{
		std::perror("connect");
		close(fd);
		return -1;
	}
	return fd;
}

/* ── Receive response until EOT ─────────────────────── */

static bool RecvResponse(int fd)
{
	char buf[4096];
	for (;;)
	{
		ssize_t n = recv(fd, buf, sizeof(buf), 0);
		if (n <= 0) return false;
		for (ssize_t i = 0; i < n; ++i)
		{
			if (buf[i] == '\x04') return true;
			std::putchar(buf[i]);
		}
	}
}

/* ── Entry point ────────────────────────────────────── */

int DebugClientMain(int argc, char *argv[])
{
	std::string socketPath;
	const char *command = nullptr;

	/* Parse args: [--socket=PATH] [command] */
	for (int i = 1; i < argc; ++i)
	{
		if (std::strncmp(argv[i], "--socket=", 9) == 0)
		{
			socketPath = argv[i] + 9;
		}
		else if (std::strcmp(argv[i], "-h") == 0 || std::strcmp(argv[i], "--help") == 0)
		{
			std::printf("Usage: maxivmac debug [--socket=PATH] [\"command\"]\n"
						"  No command: interactive mode\n"
						"  With command: one-shot mode\n");
			return 0;
		}
		else
		{
			command = argv[i];
		}
	}

	if (socketPath.empty()) socketPath = FindSocket();
	if (socketPath.empty()) return 1;

	int fd = ConnectToSocket(socketPath);
	if (fd < 0) return 1;

	if (command)
	{
		/* One-shot mode: read initial prompt, send command, read response */
		RecvResponse(fd); /* consume initial prompt + EOT */
		std::string msg = std::string(command) + "\n";
		send(fd, msg.data(), msg.size(), 0);
		RecvResponse(fd); /* read command response */
		std::fflush(stdout);
		close(fd);
		return 0;
	}

	/* Interactive mode */
	char line[1024];
	for (;;)
	{
		/* Read prompt from server (part of EOT-terminated response from prev cmd) */
		if (!RecvResponse(fd)) break;

		if (!std::fgets(line, sizeof(line), stdin)) break;

		size_t len = std::strlen(line);
		if (len == 0) continue;
		send(fd, line, len, 0);
	}
	close(fd);
	return 0;
}
