/*
	DISAssemble Motorola 68K instructions.
*/

#pragma once

#include <cstdint>
#include <string>

/* New public API: disassemble one instruction at pc, advance pc. */
std::string Disassemble(uint32_t &pc);

/* Legacy API (writes to dbglog). */
extern void DisasmOneOrSave(uint32_t pc);
extern void m68k_WantDisasmContext();

/* Dump recent saved PCs with disassembly to stderr. */
void DumpRecentDisasm();
