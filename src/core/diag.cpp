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

/* Single source of truth: the tag. CLI name = lowercase(tag). */
constexpr const char *kTags[] = {
	"EXTFS", "CLIP", "SER", "NET", "SLIP", "VID", "INIT",
};

static_assert(sizeof(kTags) / sizeof(kTags[0]) == static_cast<size_t>(DiagSubsystem::kCount));

} // namespace

const char *DiagConfig::tag(DiagSubsystem s)
{
	return kTags[static_cast<size_t>(s)];
}

bool DiagConfig::fromName(const char *name, DiagSubsystem &out)
{
	for (size_t i = 0; i < static_cast<size_t>(DiagSubsystem::kCount); ++i)
	{
		if (strcasecmp(name, kTags[i]) == 0)
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

	dbg_printf("[%s] %s", DiagConfig::tag(s), buf);
}
