/*
	diag.h — Diagnostic trace subsystem registry.

	Provides a central, runtime-togglable set of named trace channels.
	Each channel has a short tag (e.g. "ExtFS") used in log output and
	as the key for the debugger `diag` command and --diag= CLI flag.

	Usage:
	  DIAG(ExtFS, "PbOpen dir=%u name=\"%s\"", dirID, name.c_str());
*/
#pragma once

#include <bitset>
#include <cstdint>

/* Subsystem IDs — add new entries before kCount. */
enum class DiagSubsystem : uint8_t
{
	ExtFS,	  /* [EXTFS]    host-side shared drive operations   */
	Guest,	  /* [GUEST]    guest-initiated trap log reports     */
	PassThru, /* [PASSTHRU] pass-through trap reports           */
	SER,	  /* [SER]      serial port (SCC)                   */
	NET,	  /* [NET]      networking (SLIRP)                  */
	SLIP,	  /* [SLIP]     Serial Line IP                      */
	VID,	  /* [VID]      video                               */
	kCount
};

class DiagConfig
{
public:
	bool isEnabled(DiagSubsystem s) const { return enabled_.test(static_cast<size_t>(s)); }
	void set(DiagSubsystem s, bool on) { enabled_.set(static_cast<size_t>(s), on); }
	void setAll(bool on) { on ? enabled_.set() : enabled_.reset(); }

	static const char *tag(DiagSubsystem s);
	static bool fromName(const char *name, DiagSubsystem &out);

	static constexpr size_t count() { return static_cast<size_t>(DiagSubsystem::kCount); }

private:
	std::bitset<static_cast<size_t>(DiagSubsystem::kCount)> enabled_;
};

/* Global accessor — single instance, no allocation. */
DiagConfig &Diag();

/* Formatted trace output (only when subsystem is enabled). */
void DiagPrintf(DiagSubsystem s, const char *fmt, ...) __attribute__((format(printf, 2, 3)));

/* Zero-cost when disabled: arguments are not evaluated. */
#define DIAG(subsys, ...)                                                                          \
	do                                                                                             \
	{                                                                                              \
		if (Diag().isEnabled(DiagSubsystem::subsys))                                               \
			DiagPrintf(DiagSubsystem::subsys, __VA_ARGS__);                                        \
	} while (0)
