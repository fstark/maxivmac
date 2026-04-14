/*
	dbg_io.cpp — StdioIO implementation (SocketIO added in Phase 16)
*/

#include "debugger/dbg_io.h"

#include <cstdarg>
#include <cstdio>

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
