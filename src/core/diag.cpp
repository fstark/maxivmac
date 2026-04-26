/*
	diag.cpp — Diagnostic trace subsystem implementation.
*/

#include "core/diag.h"
#include "debugger/debugger.h"

#include <cstdarg>
#include <cstdio>
#include <cstring>

namespace
{

struct SubsystemInfo
{
	const char *name; /* used in CLI/debugger commands */
	const char *tag;  /* prefix in log output          */
};

constexpr SubsystemInfo kInfo[] = {
	{"extfs", "[ExtFS]"}, {"guest", "[GUEST]"}, {"sd", "[SD]"},		{"catalog", "[Catalog]"},
	{"ser", "[SER]"},	  {"net", "[NET]"},		{"slip", "[SLIP]"}, {"vid", "[VID]"},
};

static_assert(sizeof(kInfo) / sizeof(kInfo[0]) == static_cast<size_t>(DiagSubsystem::kCount));

} // namespace

const char *DiagConfig::tag(DiagSubsystem s)
{
	return kInfo[static_cast<size_t>(s)].tag;
}

bool DiagConfig::fromName(const char *name, DiagSubsystem &out)
{
	for (size_t i = 0; i < static_cast<size_t>(DiagSubsystem::kCount); ++i)
	{
		if (strcasecmp(name, kInfo[i].name) == 0)
		{
			out = static_cast<DiagSubsystem>(i);
			return true;
		}
	}
	return false;
}

static DiagConfig s_diag;

DiagConfig &Diag()
{
	return s_diag;
}

void DiagPrintf(DiagSubsystem s, const char *fmt, ...)
{
	std::va_list ap;
	va_start(ap, fmt);

	char buf[2048];
	std::vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);

	dbg_printf("%s %s", DiagConfig::tag(s), buf);
}
