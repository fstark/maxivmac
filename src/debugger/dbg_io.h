/*
	dbg_io.h — Debugger I/O abstraction

	Polymorphic interface so the command loop and handlers
	work with both stdin/stdout (--debugger) and Unix-socket
	(--debugserver) transports.
*/
#pragma once

#include <cstdarg>
#include <cstddef>
#include <memory>
#include <string>

class DbgIO
{
public:
	virtual ~DbgIO() = default;

	/* Read one line of input (blocking).  Returns false on EOF/error. */
	virtual bool readLine(char *buf, size_t len) = 0;

	/* Formatted write — printf semantics. */
	virtual void write(const char *fmt, ...) = 0;

	/* Mark end of one command's response.
	   StdioIO: no-op.  SocketIO: sends EOT byte. */
	virtual void endResponse() = 0;

	/* Flush the output stream. */
	virtual void flush() = 0;

	/* True for socket-based transports (affects disconnect handling). */
	virtual bool isSocket() const { return false; }
};

/* Create a StdioIO that wraps fgets(stdin) / vprintf(stdout). */
std::unique_ptr<DbgIO> CreateStdioIO();

/* Client entry point: maxivmac debug [...] */
int DebugClientMain(int argc, char *argv[]);

/* Create a listening Unix domain socket at the given path.
   Returns the fd, or -1 on error (message printed to stderr). */
int CreateListenSocket(const std::string &path);

/* Create a SocketIO that accepts clients on listenFd. */
std::unique_ptr<DbgIO> CreateSocketIO(int listenFd);
