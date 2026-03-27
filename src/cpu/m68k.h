#pragma once

struct MachineConfig;
extern void MINEM68K_Init(
	uint8_t *fIPL, const MachineConfig *config);
extern void m68k_IPLchangeNtfy();
extern void DiskInsertedPsuedoException(uint32_t newpc, uint32_t data);
extern void m68k_reset();

extern int32_t GetCyclesRemaining();
extern void SetCyclesRemaining(int32_t n);

extern void m68k_go_nCycles(uint32_t n);

/*
	general purpose access of address space
	of emulated computer. (memory and
	memory mapped hardware.)
*/

extern uint8_t get_vm_byte(uint32_t addr);
extern uint16_t get_vm_word(uint32_t addr);
extern uint32_t get_vm_long(uint32_t addr);

extern void put_vm_byte(uint32_t addr, uint8_t b);
extern void put_vm_word(uint32_t addr, uint16_t w);
extern void put_vm_long(uint32_t addr, uint32_t l);

extern void SetHeadATTel(ATTep p);
extern ATTep FindATTel(uint32_t addr);
