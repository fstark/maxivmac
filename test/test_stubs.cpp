/*
	test_stubs.cpp — Link stubs for symbols needed by test-only dependencies.

	These provide minimal definitions for symbols pulled in by
	lomem_globals.cpp, trap_counter.cpp, and trap_tracer.cpp that
	aren't needed for unit testing the debugger modules.
*/

#include <cstdint>
#include <cstring>
#include <cstdarg>
#include <cstdio>

/* ── Guest memory stub ────────────────────────────────── */

/* trap_tracer.cpp reads from guest memory via get_vm_byte/word/long.
   In the test binary these just read from g_ram[] (big-endian). */
uint8_t g_ram[4096] = {};

uint8_t get_vm_byte(uint32_t addr)
{
	if (addr < sizeof(g_ram)) return g_ram[addr];
	return 0;
}

uint16_t get_vm_word(uint32_t addr)
{
	if (addr + 1 < sizeof(g_ram))
		return static_cast<uint16_t>((g_ram[addr] << 8) | g_ram[addr + 1]);
	return 0;
}

uint32_t get_vm_long(uint32_t addr)
{
	if (addr + 3 < sizeof(g_ram))
		return (static_cast<uint32_t>(g_ram[addr]) << 24) |
			   (static_cast<uint32_t>(g_ram[addr + 1]) << 16) |
			   (static_cast<uint32_t>(g_ram[addr + 2]) << 8) |
			   static_cast<uint32_t>(g_ram[addr + 3]);
	return 0;
}

/* ── CPU register stubs ──────────────────────────────── */

/* Settable from tests via test_set_regs(). */
static uint32_t s_dregs[8] = {};
static uint32_t s_aregs[8] = {};
static uint32_t s_pc = 0;

uint32_t g_instructionCount = 0;

void m68k_getRegs(uint32_t *d, uint32_t *a)
{
	memcpy(d, s_dregs, sizeof(s_dregs));
	memcpy(a, s_aregs, sizeof(s_aregs));
}

uint32_t m68k_getPC_public()
{
	return s_pc;
}

/* Test helpers — set CPU state before calling tracer */
void test_set_regs(const uint32_t d[8], const uint32_t a[8])
{
	memcpy(s_dregs, d, sizeof(s_dregs));
	memcpy(s_aregs, a, sizeof(s_aregs));
}

void test_set_pc(uint32_t pc)
{
	s_pc = pc;
}

/* ── MacRoman stubs (lomem_globals.cpp) ───────────────── */

extern "C++"
{
	unsigned int MacRoman2UniCodeSize(unsigned char *, unsigned int)
	{
		return 0;
	}
	void MacRoman2UniCodeData(unsigned char *, unsigned int, char *) {}
}

/* ── Debug output stub ────────────────────────────────── */

void dbg_printf(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	std::vfprintf(stderr, fmt, ap);
	va_end(ap);
}
