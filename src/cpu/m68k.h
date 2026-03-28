/*
	m68k — Motorola 68000-series CPU emulator interface

	Declares the public API for the 68K interpreter core.
	Covers initialization, reset, interrupt delivery, cycle-counted
	execution, and guest address-space access.
*/
#pragma once

struct MachineConfig;

/* --- Initialization and control --- */

// Set up CPU state, instruction dispatch table, and IPL pointer.
extern void MINEM68K_Init(
	uint8_t *fIPL, const MachineConfig *config);

// Notify CPU that the external interrupt priority level changed.
extern void m68k_IPLchangeNtfy();

// Inject a pseudo-exception to deliver a disk-insertion event to the guest OS.
extern void DiskInsertedPsuedoException(uint32_t newpc, uint32_t data);

// Full CPU reset: load SP/PC from vectors, enter supervisor mode.
extern void m68k_reset();

/* --- Cycle accounting --- */

// Return total CPU cycles remaining in the current timeslice.
extern int32_t GetCyclesRemaining();

// Set total CPU cycles remaining in the current timeslice.
extern void SetCyclesRemaining(int32_t n);

// Run the CPU for approximately n cycles.
extern void m68k_go_nCycles(uint32_t n);

/* --- Guest address-space access (memory and memory-mapped I/O) --- */

extern uint8_t get_vm_byte(uint32_t addr);
extern uint16_t get_vm_word(uint32_t addr);
extern uint32_t get_vm_long(uint32_t addr);

extern void put_vm_byte(uint32_t addr, uint8_t b);
extern void put_vm_word(uint32_t addr, uint16_t w);
extern void put_vm_long(uint32_t addr, uint32_t l);

/* --- Address Translation Table (ATT) --- */

// Install a new ATT list head and invalidate the translation cache.
extern void SetHeadATTel(ATTep p);

// Search ATT for the entry matching addr, with LRU reordering.
extern ATTep FindATTel(uint32_t addr);
