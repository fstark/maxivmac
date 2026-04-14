/*
	test_stubs.cpp — Link stubs for symbols needed by test-only dependencies.

	These provide minimal definitions for symbols pulled in by
	lomem_globals.cpp and trap_counter.cpp that aren't needed for
	unit testing the debugger modules.
*/

#include <cstdint>
#include <cstring>

/* lomem_globals.cpp reads from g_ram for value formatting */
uint8_t g_ram[4096] = {};

/* lomem_globals.cpp uses MacRoman conversion */
extern "C++"
{
	unsigned int MacRoman2UniCodeSize(unsigned char *, unsigned int) { return 0; }
	void MacRoman2UniCodeData(unsigned char *, unsigned int, char *) {}
}

/*
	trap_counter.cpp calls g_tracer.enable(bool).
	We need to provide g_trapDefs + g_tracer with the right types
	from trap_tracer.h — but TrapTracer's methods link to m68k.cpp etc.
	Solution: include the header but provide our own minimal TrapTracer
	member definitions right here.
*/
#include "cpu/trap_defs.h"

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-private-field"
#include "cpu/trap_tracer.h"
#pragma clang diagnostic pop

TrapDefs g_trapDefs;

/* Provide just the methods that get called from trap_counter.cpp */
TrapTracer::TrapTracer(TrapDefs &defs) : defs_(defs) {}

void TrapTracer::enable(bool on)
{
	enabled_ = on;
}

/* The remaining TrapTracer methods are stubs to satisfy the linker.
   They're never called in the test binary. */
void TrapTracer::enter(uint16_t) {}
void TrapTracer::checkReturn(uint32_t) {}
void TrapTracer::setMaxDepth(int) {}
void TrapTracer::addFilter(uint16_t) {}
void TrapTracer::clearFilter() {}
void TrapTracer::flushStack(const char *) {}
void TrapTracer::emitEntry(const TrapFrame &, const TrapDef &) {}
void TrapTracer::emitEntry(const TrapFrame &) {}
void TrapTracer::emitAutoPop(const TrapFrame &, const char *) {}
void TrapTracer::emitExit(const TrapFrame &, const TrapDef &) {}
void TrapTracer::emitExit(const TrapFrame &) {}
uint32_t TrapTracer::readParamRaw(const ParamDef &, uint32_t, int &) { return 0; }
uint16_t TrapTracer::readAppId() const { return 0; }
std::string TrapTracer::readAppName() const { return {}; }
std::string TrapTracer::formatParam(const ParamDef &, uint32_t) { return {}; }
std::string TrapTracer::formatOSType(uint32_t) { return {}; }
std::string TrapTracer::formatStr255(uint32_t) { return {}; }
std::string TrapTracer::formatOSErr(int16_t) { return {}; }

TrapTracer g_tracer(g_trapDefs);
